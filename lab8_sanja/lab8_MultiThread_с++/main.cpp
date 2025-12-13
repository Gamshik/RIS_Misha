#include <windows.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>

struct ThreadData
{
    int threadId;
    int startRow;
    int endRow;
    int N;
    double* Aflat;
    double* Lflat;
    double* Uflat;
    double* D;
    volatile int currentI;
    HANDLE startEvent;
    HANDLE doneEvent;
    volatile bool terminate;
};

void printW(const std::wstring& w)
{
    DWORD written;
    WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE),
                  w.c_str(),
                  (DWORD)w.size(),
                  &written,
                  nullptr);
}

std::wstring utf8_to_wstring(const std::string& s)
{
    if (s.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], size_needed);
    return w;
}

DWORD WINAPI LDUWorker(LPVOID lpParam)
{
    ThreadData* data = (ThreadData*)lpParam;
    int N = data->N;

    while (!data->terminate)
    {
        WaitForSingleObject(data->startEvent, INFINITE);
        if (data->terminate) break;

        int i = data->currentI;

        // Вычисление U[i,j] для j >= i
        for (int j = std::max(i, data->startRow); j < data->endRow && j < N; ++j)
        {
            double sumU = 0;
            for (int k = 0; k < i; ++k)
                sumU += data->Lflat[i * N + k] * data->D[k] * data->Uflat[k * N + j];
            data->Uflat[i * N + j] = data->Aflat[i * N + j] - sumU;
        }

        // Вычисление L[j,i] для j > i
        for (int j = std::max(i + 1, data->startRow); j < data->endRow && j < N; ++j)
        {
            double sumL = 0;
            for (int k = 0; k < i; ++k)
                sumL += data->Lflat[j * N + k] * data->D[k] * data->Uflat[k * N + i];
            data->Lflat[j * N + i] = (data->Aflat[j * N + i] - sumL) / data->D[i];
        }

        SetEvent(data->doneEvent);
    }

    return 0;
}

void ReadMatrixAndVector(const std::string& folder, std::vector<double>& Aflat, std::vector<double>& B, int& N)
{
    std::ifstream finA(folder + "\\A.txt");
    if (!finA.is_open()) throw std::runtime_error("Не удалось открыть A.txt");

    std::string line;
    std::vector<std::vector<double>> tempA;

    while (std::getline(finA, line))
    {
        std::istringstream ss(line);
        std::vector<double> row;
        double val;
        while (ss >> val) row.push_back(val);
        if (!row.empty()) tempA.push_back(row);
    }
    finA.close();

    N = (int)tempA.size();
    if (N == 0) throw std::runtime_error("Матрица A пустая");

    Aflat.resize(N * N);
    for (int i = 0; i < N; ++i)
        for (int j = 0; j < N; ++j)
            Aflat[i * N + j] = tempA[i][j];

    std::ifstream finB(folder + "\\B.txt");
    if (!finB.is_open()) throw std::runtime_error("Не удалось открыть B.txt");
    B.resize(N);
    for (int i = 0; i < N; ++i)
        if (!(finB >> B[i])) throw std::runtime_error("B.txt не соответствует размерам");
    finB.close();
}

void WriteVector(const std::string& path, const std::vector<double>& X)
{
    std::ofstream fout(path);
    fout << std::setprecision(17);
    for (double v : X) fout << v << "\n";
    fout.close();
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        printW(L"Ожидался аргумент — путь к папке с данными.\n");
        return 1;
    }

    std::string folder = argv[1];
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    int numThreads = (int)sysinfo.dwNumberOfProcessors;
    if (argc >= 3) { int t = atoi(argv[2]); if (t > 0) numThreads = t; }

    std::vector<double> Aflat, B;
    int N;
    try { ReadMatrixAndVector(folder, Aflat, B, N); }
    catch (const std::exception& e)
    {
        printW(L"Ошибка при чтении входных файлов: " + utf8_to_wstring(e.what()) + L"\n");
        return 1;
    }

    std::vector<double> Lflat(N * N, 0.0), Uflat(N * N, 0.0), D(N, 0.0);
    std::vector<ThreadData> tdata(numThreads);
    std::vector<HANDLE> threads(numThreads);
    std::vector<HANDLE> startEvents(numThreads), doneEvents(numThreads);
    int rowsPerThread = (N + numThreads - 1) / numThreads;

    for (int t = 0; t < numThreads; ++t)
    {
        int startRow = t * rowsPerThread;
        int endRow = std::min((t + 1) * rowsPerThread, N);
        startEvents[t] = CreateEvent(NULL, FALSE, FALSE, NULL);
        doneEvents[t] = CreateEvent(NULL, FALSE, FALSE, NULL);
        tdata[t] = { t, startRow, endRow, N, Aflat.data(), Lflat.data(), Uflat.data(), D.data(), -1, startEvents[t], doneEvents[t], false };
        threads[t] = CreateThread(NULL, 0, LDUWorker, &tdata[t], 0, NULL);
    }

    auto t1 = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < N; ++i)
    {
        Lflat[i * N + i] = 1.0;
        double sumDiag = Aflat[i * N + i];
        for (int k = 0; k < i; ++k)
            sumDiag -= Lflat[i * N + k] * D[k] * Uflat[k * N + i];
        D[i] = sumDiag;

        for (int t = 0; t < numThreads; ++t)
            tdata[t].currentI = i;
        for (int t = 0; t < numThreads; ++t) SetEvent(startEvents[t]);
        WaitForMultipleObjects(numThreads, doneEvents.data(), TRUE, INFINITE);
    }

    std::vector<double> Z(N);
    for (int i = 0; i < N; ++i)
    {
        double sum = B[i];
        for (int j = 0; j < i; ++j)
            sum -= Lflat[i * N + j] * Z[j];
        Z[i] = sum;
    }

    std::vector<double> Y(N);
    for (int i = 0; i < N; ++i) Y[i] = Z[i] / D[i];

    std::vector<double> X(N);
    for (int i = N - 1; i >= 0; --i)
    {
        double sum = Y[i];
        for (int j = i + 1; j < N; ++j)
            sum -= Uflat[i * N + j] * X[j];
        X[i] = sum;
    }

    auto t2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = t2 - t1;

    WriteVector(folder + "\\X.txt", X);

    printW(L"==============================================\n");
    printW(L"Размер матрицы: " + std::to_wstring(N) + L"x" + std::to_wstring(N) + L"\n");
    printW(L"Потоков: " + std::to_wstring(numThreads) + L"\n");
    printW(L"Время: " + std::to_wstring(elapsed.count()) + L" мс\n");
    printW(L"==============================================\n");

    for (int t = 0; t < numThreads; ++t)
    {
        tdata[t].terminate = true;
        SetEvent(startEvents[t]);
    }
    WaitForMultipleObjects(numThreads, threads.data(), TRUE, INFINITE);
    for (int t = 0; t < numThreads; ++t)
    {
        CloseHandle(threads[t]);
        CloseHandle(startEvents[t]);
        CloseHandle(doneEvents[t]);
    }

    return 0;
}

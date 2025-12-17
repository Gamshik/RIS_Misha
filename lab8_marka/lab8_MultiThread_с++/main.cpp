#include <windows.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>

// ==========================================================
// Unicode-вывод в консоль (кириллица)
void printW(const std::wstring& w)
{
    DWORD written;
    WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE),
                  w.c_str(),
                  (DWORD)w.size(),
                  &written,
                  nullptr);
}

// ==========================================================
// Данные потока
struct ThreadData
{
    int threadId;
    int startCol;
    int endCol;
    int N;

    double* Aflat;

    std::vector<std::vector<double>>* Ldiag; // L[d][j] = L[j+d][j]
    std::vector<std::vector<double>>* Udiag; // U[d][i] = U[i][i+d]

    volatile int currentI;
    HANDLE startEvent;
    HANDLE doneEvent;
    volatile bool terminate;
};

// ==========================================================
// Доступ к диагоналям
inline double Uat(const std::vector<std::vector<double>>& U, int i, int j)
{
    return U[j - i][i]; // j >= i
}
inline void Uset(std::vector<std::vector<double>>& U, int i, int j, double v)
{
    U[j - i][i] = v;
}

inline double Lat(const std::vector<std::vector<double>>& L, int i, int j)
{
    return (i == j) ? 1.0 : L[i - j][j];
}
inline void Lset(std::vector<std::vector<double>>& L, int i, int j, double v)
{
    L[i - j][j] = v;
}

// ==========================================================
// Поток LU
DWORD WINAPI LUWorker(LPVOID lpParam)
{
    ThreadData* data = (ThreadData*)lpParam;
    int N = data->N;

    while (!data->terminate)
    {
        WaitForSingleObject(data->startEvent, INFINITE);
        if (data->terminate) break;

        int i = data->currentI;

        // ---------- U[i,j], j >= i ----------
        for (int j = std::max(i, data->startCol); j < data->endCol && j < N; ++j)
        {
            double sum = 0.0;
            for (int k = 0; k < i; ++k)
                sum += Lat(*data->Ldiag, i, k) * Uat(*data->Udiag, k, j);

            Uset(*data->Udiag, i, j, data->Aflat[i * N + j] - sum);
        }

        // ---------- L[j,i], j > i ----------
        for (int j = std::max(i + 1, data->startCol); j < data->endCol && j < N; ++j)
        {
            double sum = 0.0;
            for (int k = 0; k < i; ++k)
                sum += Lat(*data->Ldiag, j, k) * Uat(*data->Udiag, k, i);

            Lset(*data->Ldiag, j, i,
                 (data->Aflat[j * N + i] - sum) / Uat(*data->Udiag, i, i));
        }

        SetEvent(data->doneEvent);
    }
    return 0;
}

// ==========================================================
// Чтение матрицы и вектора
bool ReadMatrix(const std::string& folder,
                std::vector<double>& A,
                std::vector<double>& B,
                int& N)
{
    std::ifstream fa(folder + "\\A.txt");
    if (!fa.is_open()) return false;

    std::vector<std::vector<double>> tmp;
    std::string line;
    while (std::getline(fa, line))
    {
        std::istringstream ss(line);
        std::vector<double> row;
        double v;
        while (ss >> v) row.push_back(v);
        if (!row.empty()) tmp.push_back(row);
    }
    fa.close();

    N = (int)tmp.size();
    A.resize(N * N);
    for (int i = 0; i < N; ++i)
        for (int j = 0; j < N; ++j)
            A[i * N + j] = tmp[i][j];

    std::ifstream fb(folder + "\\B.txt");
    if (!fb.is_open()) return false;

    B.resize(N);
    for (int i = 0; i < N; ++i) fb >> B[i];
    fb.close();

    return true;
}

// ==========================================================
// Запись векторного решения
void WriteVector(const std::string& path, const std::vector<double>& X)
{
    std::ofstream f(path);
    f << std::setprecision(17);
    for (double v : X) f << v << "\n";
    f.close();
}

// ==========================================================
// Main
int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        printW(L"Ожидался аргумент — путь к папке с данными.\n");
        return 1;
    }

    std::string folder = argv[1];

    SYSTEM_INFO si;
    GetSystemInfo(&si);
    int numThreads = si.dwNumberOfProcessors;
    if (argc >= 3)
    {
        int t = atoi(argv[2]);
        if (t > 0) numThreads = t;
    }

    std::vector<double> A, B;
    int N;
    if (!ReadMatrix(folder, A, B, N))
    {
        printW(L"Ошибка чтения входных файлов.\n");
        return 1;
    }

    // ---------- диагональное хранение ----------
    std::vector<std::vector<double>> Ldiag(N), Udiag(N);
    for (int d = 0; d < N; ++d)
    {
        Udiag[d].resize(N - d);
        if (d > 0) Ldiag[d].resize(N - d);
    }

    std::vector<ThreadData> tdata(numThreads);
    std::vector<HANDLE> threads(numThreads);
    std::vector<HANDLE> startEv(numThreads), doneEv(numThreads);

    int colsPerThread = (N + numThreads - 1) / numThreads;

    for (int t = 0; t < numThreads; ++t)
    {
        int sc = t * colsPerThread;
        int ec = std::min(sc + colsPerThread, N);

        startEv[t] = CreateEvent(NULL, FALSE, FALSE, NULL);
        doneEv[t]  = CreateEvent(NULL, FALSE, FALSE, NULL);

        tdata[t] = { t, sc, ec, N, A.data(), &Ldiag, &Udiag, -1, startEv[t], doneEv[t], false };
        threads[t] = CreateThread(NULL, 0, LUWorker, &tdata[t], 0, NULL);
    }

    auto t1 = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < N; ++i)
    {
        for (int t = 0; t < numThreads; ++t)
            tdata[t].currentI = i;

        for (int t = 0; t < numThreads; ++t)
            SetEvent(startEv[t]);

        WaitForMultipleObjects(numThreads, doneEv.data(), TRUE, INFINITE);
    }

    // ---------- прямой ход ----------
    std::vector<double> Z(N);
    for (int i = 0; i < N; ++i)
    {
        double sum = B[i];
        for (int j = 0; j < i; ++j)
            sum -= Lat(Ldiag, i, j) * Z[j];
        Z[i] = sum;
    }

    // ---------- обратный ход ----------
    std::vector<double> X(N);
    for (int i = N - 1; i >= 0; --i)
    {
        double sum = Z[i];
        for (int j = i + 1; j < N; ++j)
            sum -= Uat(Udiag, i, j) * X[j];
        X[i] = sum / Uat(Udiag, i, i);
    }

    auto t2 = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration<double, std::milli>(t2 - t1).count();

    WriteVector(folder + "\\X.txt", X);

    printW(L"==============================================\n");
    printW(L"Размер матрицы: " + std::to_wstring(N) + L"x" + std::to_wstring(N) + L"\n");
    printW(L"Потоков: " + std::to_wstring(numThreads) + L"\n");
    printW(L"Время: " + std::to_wstring(ms) + L" мс\n");
    printW(L"==============================================\n");

    // ---------- завершение потоков ----------
    for (int t = 0; t < numThreads; ++t)
    {
        tdata[t].terminate = true;
        SetEvent(startEv[t]);
    }
    WaitForMultipleObjects(numThreads, threads.data(), TRUE, INFINITE);
    for (int t = 0; t < numThreads; ++t)
    {
        CloseHandle(threads[t]);
        CloseHandle(startEv[t]);
        CloseHandle(doneEv[t]);
    }

    return 0;
}

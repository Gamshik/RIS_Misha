// CholeskyMultiThread.cpp
// Многопоточный Cholesky (U^T * U) с полнопрофильным хранением.
// Использует Windows threads и printW для корректного вывода кириллицы.

#include <windows.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <cmath>
#include <algorithm>

struct ThreadData
{
    int threadId;
    int startRow;
    int endRow;
    int N;
    double* Aflat;      // исходная матрица A (полный массив, row-major)
    double* Uflat;      // верхнетреугольная матрица U (полный массив, row-major), заполняется по строкам
    volatile int currentK;    // текущий k (задаётся ведущим потоком)
    volatile double akk;      // U[k,k]
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


DWORD WINAPI CholeskyWorker(LPVOID lpParam)
{
    ThreadData* data = (ThreadData*)lpParam;

    int N = data->N;

    while (!data->terminate)
    {
        // ждём сигнала на старт итерации
        WaitForSingleObject(data->startEvent, INFINITE);

        if (data->terminate) break;

        int k = data->currentK;
        double akk = data->akk;

        // Обработка только для i > k
        int start = std::max(data->startRow, k + 1);
        int end = data->endRow;

        for (int i = start; i < end; ++i)
        {
            // считаем s = A[k,i] - sum_{p=0..k-1} U[p,k] * U[p,i]
            double s = data->Aflat[k * N + i];
            // суммирование по p
            for (int p = 0; p < k; ++p)
            {
                double upk = data->Uflat[p * N + k];
                double upi = data->Uflat[p * N + i];
                s -= upk * upi;
            }
            // записываем U[k,i]
            data->Uflat[k * N + i] = s / akk;
        }

        // сигнал, что эта порция выполнена
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

    // проверка правильности размерности
    for (int i = 0; i < N; ++i)
        if ((int)tempA[i].size() != N)
            throw std::runtime_error("A.txt: некорректная размерность строки");

    Aflat.resize(N * N);
    for (int i = 0; i < N; ++i)
        for (int j = 0; j < N; ++j)
            Aflat[i * N + j] = tempA[i][j];

    std::ifstream finB(folder + "\\B.txt");
    if (!finB.is_open()) throw std::runtime_error("Не удалось открыть B.txt");

    B.resize(N);
    for (int i = 0; i < N; ++i)
    {
        if (!(finB >> B[i]))
            throw std::runtime_error("B.txt не соответствует размерам");
    }
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

    // получение количества процессоров
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    int numThreads = (int)sysinfo.dwNumberOfProcessors;

    // Если пользователь указал число потоков вторым аргументом - используем его
    if (argc >= 3)
    {
        int t = atoi(argv[2]);
        if (t > 0) numThreads = t;
    }

    std::vector<double> Aflat;
    std::vector<double> B;
    int N;
    try
    {
        ReadMatrixAndVector(folder, Aflat, B, N);
    }
    catch (const std::exception& ex)
    {
        std::wstring msg = L"Ошибка при чтении входных файлов: ";
        msg += utf8_to_wstring(ex.what());
        msg += L"\n";
        printW(msg);
        return 1;
    }

    // выделяем Uflat (полный массив), предварительно заполнен нулями
    std::vector<double> Uflat((size_t)N * N, 0.0);

    // подготовка потоков
    std::vector<ThreadData> tdata(numThreads);
    std::vector<HANDLE> threads(numThreads);
    std::vector<HANDLE> startEvents(numThreads);
    std::vector<HANDLE> doneEvents(numThreads);

    int rowsPerThread = (N + numThreads - 1) / numThreads;

    for (int t = 0; t < numThreads; ++t)
    {
        int startRow = t * rowsPerThread;
        int endRow = std::min((t + 1) * rowsPerThread, N);

        startEvents[t] = CreateEvent(NULL, FALSE, FALSE, NULL); // auto-reset
        doneEvents[t]  = CreateEvent(NULL, FALSE, FALSE, NULL); // auto-reset

        tdata[t].threadId = t;
        tdata[t].startRow = startRow;
        tdata[t].endRow = endRow;
        tdata[t].N = N;
        tdata[t].Aflat = Aflat.data();
        tdata[t].Uflat = Uflat.data();
        tdata[t].currentK = -1;
        tdata[t].akk = 0.0;
        tdata[t].startEvent = startEvents[t];
        tdata[t].doneEvent = doneEvents[t];
        tdata[t].terminate = false;

        threads[t] = CreateThread(NULL, 0, CholeskyWorker, &tdata[t], 0, NULL);
        if (!threads[t])
        {
            printW(L"Не удалось создать поток\n");
            return 1;
        }
    }

    // Главный цикл Cholesky
    auto t1 = std::chrono::high_resolution_clock::now();

    try
    {
        for (int k = 0; k < N; ++k)
        {
            // диагональный элемент
            double sum_diag = Aflat[k * N + k];
            for (int p = 0; p < k; ++p)
            {
                double upk = Uflat[p * N + k];
                sum_diag -= upk * upk;
            }

            if (sum_diag <= 0.0)
            {
                // освобождаем потоки корректно перед исключением
                for (int t = 0; t < numThreads; ++t) tdata[t].terminate = true;
                for (int t = 0; t < numThreads; ++t) SetEvent(startEvents[t]);
                WaitForMultipleObjects(numThreads, threads.data(), TRUE, INFINITE);
                for (int t = 0; t < numThreads; ++t)
                {
                    CloseHandle(threads[t]);
                    CloseHandle(startEvents[t]);
                    CloseHandle(doneEvents[t]);
                }

                throw std::runtime_error("Матрица не является положительно определенной");
            }

            double akk = std::sqrt(sum_diag);
            Uflat[k * N + k] = akk;

            // Устанавливаем текущий k и akk в данные потоков
            for (int t = 0; t < numThreads; ++t)
            {
                tdata[t].currentK = k;
                tdata[t].akk = akk;
            }

            // Запускаем потоки на обработку строк i > k
            for (int t = 0; t < numThreads; ++t)
                SetEvent(startEvents[t]);

            // Ожидаем завершения всех потоков (doneEvents)
            WaitForMultipleObjects(numThreads, doneEvents.data(), TRUE, INFINITE);
        }
    }
    catch (const std::exception& ex)
    {
        std::wstring msg = L"Ошибка во время разложения: ";
        msg += utf8_to_wstring(ex.what());
        msg += L"\n";
        printW(msg);
        // корректное завершение потоков
        for (int t = 0; t < numThreads; ++t) tdata[t].terminate = true;
        for (int t = 0; t < numThreads; ++t) SetEvent(startEvents[t]);
        WaitForMultipleObjects(numThreads, threads.data(), TRUE, INFINITE);
        for (int t = 0; t < numThreads; ++t)
        {
            CloseHandle(threads[t]);
            CloseHandle(startEvents[t]);
            CloseHandle(doneEvents[t]);
        }
        return 1;
    }

    // Завершаем воркеры корректно
    for (int t = 0; t < numThreads; ++t)
        tdata[t].terminate = true;
    for (int t = 0; t < numThreads; ++t)
        SetEvent(startEvents[t]);
    WaitForMultipleObjects(numThreads, threads.data(), TRUE, INFINITE);

    // Закрываем дескрипторы потоков и событий
    for (int t = 0; t < numThreads; ++t)
    {
        CloseHandle(threads[t]);
        CloseHandle(startEvents[t]);
        CloseHandle(doneEvents[t]);
    }

    // Решение системы: сначала U^T * Y = B (forward), потом U * X = Y (backward)
    std::vector<double> Y(N);
    for (int i = 0; i < N; ++i)
    {
        double s = B[i];
        for (int p = 0; p < i; ++p)
        {
            s -= Uflat[p * N + i] * Y[p];
        }
        Y[i] = s / Uflat[i * N + i];
    }

    std::vector<double> X(N);
    for (int i = N - 1; i >= 0; --i)
    {
        double s = Y[i];
        for (int j = i + 1; j < N; ++j)
            s -= Uflat[i * N + j] * X[j];
        X[i] = s / Uflat[i * N + i];
    }

    auto t2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = t2 - t1;

    WriteVector(folder + "\\X.txt", X);

    printW(L"==============================================\n");
    printW(std::wstring(L"Размер матрицы: ") + std::to_wstring(N) + L"x" + std::to_wstring(N) + L"\n");
    printW(std::wstring(L"Потоков: ") + std::to_wstring(numThreads) + L"\n");
    printW(std::wstring(L"Время: ") + std::to_wstring(elapsed.count()) + L" мс\n");
    printW(L"==============================================\n");

    return 0;
}

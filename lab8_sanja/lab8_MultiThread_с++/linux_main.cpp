#include <pthread.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <stdexcept>

struct WorkerInfo {
    int id;
    int startRow;
    int endRow;
};

static std::vector<double> Aflat;
static std::vector<double> Lflat;
static std::vector<double> Uflat;
static std::vector<double> D;
static std::vector<double> B;
static int N;
static int numThreads;

static std::vector<WorkerInfo> workers;
static std::vector<pthread_t> threads;

static pthread_mutex_t mutex_shared = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cvStart = PTHREAD_COND_INITIALIZER;
static pthread_cond_t cvDone  = PTHREAD_COND_INITIALIZER;

static int current_i = -1;
static bool terminateFlag = false;
static int finishedCount = 0;

void ReadMatrixAndVector(const std::string& folder, std::vector<double>& Af, std::vector<double>& Bv, int& n)
{
    std::ifstream finA(folder + "/A.txt");
    if (!finA) throw std::runtime_error("Cannot open A.txt");
    std::string line;
    std::vector<std::vector<double>> tmp;
    while (std::getline(finA, line))
    {
        std::istringstream ss(line);
        std::vector<double> row;
        double v;
        while (ss >> v) row.push_back(v);
        if (!row.empty()) tmp.push_back(std::move(row));
    }
    n = (int)tmp.size();
    if (n == 0) throw std::runtime_error("Empty A.txt");
    Af.assign(n * n, 0.0);
    for (int i = 0; i < n; ++i)
    {
        if ((int)tmp[i].size() != n)
            throw std::runtime_error("A.txt: inconsistent row size");
        for (int j = 0; j < n; ++j) Af[i * n + j] = tmp[i][j];
    }

    std::ifstream finB(folder + "/B.txt");
    if (!finB) throw std::runtime_error("Cannot open B.txt");
    Bv.assign(n, 0.0);
    for (int i = 0; i < n; ++i) {
        if (!(finB >> Bv[i])) throw std::runtime_error("B.txt not match size");
    }
}

void WriteVector(const std::string& path, const std::vector<double>& X)
{
    std::ofstream fout(path);
    fout << std::setprecision(17);
    for (double v : X) fout << v << "\n";
}

void* WorkerRoutine(void* arg)
{
    WorkerInfo* wi = (WorkerInfo*)arg;

    while (true)
    {
        pthread_mutex_lock(&mutex_shared);
        while (current_i == -1 && !terminateFlag) {
            pthread_cond_wait(&cvStart, &mutex_shared);
        }
        if (terminateFlag) {
            pthread_mutex_unlock(&mutex_shared);
            break;
        }
        int i = current_i;
        pthread_mutex_unlock(&mutex_shared);

        // Вычисляем U[i,j] для j >= i
        for (int j = std::max(i, wi->startRow); j < wi->endRow && j < N; ++j)
        {
            double sumU = 0.0;
            for (int k = 0; k < i; ++k)
                sumU += Lflat[i * N + k] * D[k] * Uflat[k * N + j];
            Uflat[i * N + j] = Aflat[i * N + j] - sumU;
        }

        // Вычисляем L[j,i] для j > i
        for (int j = std::max(i + 1, wi->startRow); j < wi->endRow && j < N; ++j)
        {
            double sumL = 0.0;
            for (int k = 0; k < i; ++k)
                sumL += Lflat[j * N + k] * D[k] * Uflat[k * N + i];
            Lflat[j * N + i] = (Aflat[j * N + i] - sumL) / D[i];
        }

        pthread_mutex_lock(&mutex_shared);
        finishedCount++;
        if (finishedCount >= numThreads) pthread_cond_signal(&cvDone);
        pthread_mutex_unlock(&mutex_shared);
    }

    return nullptr;
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "Ожидался аргумент — путь к папке с данными\n";
        return 1;
    }
    std::string folder = argv[1];

    numThreads = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (numThreads <= 0) numThreads = 1;
    if (argc >= 3) { int t = atoi(argv[2]); if (t > 0) numThreads = t; }

    try { ReadMatrixAndVector(folder, Aflat, B, N); }
    catch (const std::exception& e) { std::cerr << "Ошибка при чтении входных файлов: " << e.what() << "\n"; return 1; }

    Lflat.assign(N * N, 0.0);
    Uflat.assign(N * N, 0.0);
    D.assign(N, 0.0);

    for (int i = 0; i < N; ++i) Lflat[i * N + i] = 1.0; // диагональ L*

    workers.resize(numThreads);
    threads.resize(numThreads);
    int rowsPerThread = (N + numThreads - 1) / numThreads;
    for (int t = 0; t < numThreads; ++t)
    {
        int start = t * rowsPerThread;
        int end = std::min((t + 1) * rowsPerThread, N);
        workers[t] = { t, start, end };
        pthread_create(&threads[t], nullptr, WorkerRoutine, &workers[t]);
    }

    auto t1 = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < N; ++i)
    {
        // Вычисляем диагональ D[i]
        double sumDiag = Aflat[i * N + i];
        for (int k = 0; k < i; ++k)
            sumDiag -= Lflat[i * N + k] * D[k] * Uflat[k * N + i];
        D[i] = sumDiag;

        pthread_mutex_lock(&mutex_shared);
        current_i = i;
        finishedCount = 0;
        pthread_cond_broadcast(&cvStart);
        while (finishedCount < numThreads) pthread_cond_wait(&cvDone, &mutex_shared);
        current_i = -1;
        pthread_mutex_unlock(&mutex_shared);
    }

    pthread_mutex_lock(&mutex_shared);
    terminateFlag = true;
    pthread_cond_broadcast(&cvStart);
    pthread_mutex_unlock(&mutex_shared);
    for (int t = 0; t < numThreads; ++t) pthread_join(threads[t], nullptr);

    // Решение L* Z = B
    std::vector<double> Z(N);
    for (int i = 0; i < N; ++i)
    {
        double sum = B[i];
        for (int j = 0; j < i; ++j) sum -= Lflat[i * N + j] * Z[j];
        Z[i] = sum;
    }

    // D * Y = Z
    std::vector<double> Y(N);
    for (int i = 0; i < N; ++i) Y[i] = Z[i] / D[i];

    // U* X = Y
    std::vector<double> X(N);
    for (int i = N - 1; i >= 0; --i)
    {
        double sum = Y[i];
        for (int j = i + 1; j < N; ++j) sum -= Uflat[i * N + j] * X[j];
        X[i] = sum;
    }

    auto t2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = t2 - t1;

    WriteVector(folder + "/X.txt", X);

    std::cout << "==============================================\n";
    std::cout << "Размер матрицы: " << N << "x" << N << "\n";
    std::cout << "Потоков: " << numThreads << "\n";
    std::cout << "Время: " << elapsed.count() << " мс\n";
    std::cout << "==============================================\n";

    return 0;
}

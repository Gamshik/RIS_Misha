// cholesky_pthreads.cpp
// Multithreaded Cholesky (U^T * U) using pthreads, full matrix storage (row-major).

#include <pthread.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <stdexcept>

struct WorkerInfo {
    int id;
    int startIdx; // inclusive index i (column/row index) start for processing U[k,i]
    int endIdx;   // exclusive index
};

static std::vector<double> Aflat;
static std::vector<double> B;
static std::vector<double> Uflat; // upper triangular matrix U stored full (row-major)
static int N;
static int numThreads;

static std::vector<WorkerInfo> workers;
static std::vector<pthread_t> threads;

static pthread_mutex_t mutex_shared = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cvStart = PTHREAD_COND_INITIALIZER;
static pthread_cond_t cvDone  = PTHREAD_COND_INITIALIZER;

static int current_k = -1;
static double shared_akk = 0.0;
static int finishedCount = 0;
static bool terminateFlag = false;

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
    for (int i = 0; i < n; ++i) {
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
        while (current_k == -1 && !terminateFlag) {
            pthread_cond_wait(&cvStart, &mutex_shared);
        }

        if (terminateFlag) {
            pthread_mutex_unlock(&mutex_shared);
            break;
        }

        int k = current_k;
        double akk = shared_akk;
        pthread_mutex_unlock(&mutex_shared);

        // Each worker computes U[k,i] for i in [startIdx, endIdx)
        int start = std::max(wi->startIdx, k + 1);
        int end = wi->endIdx;
        if (start < end) {
            for (int i = start; i < end; ++i) {
                // compute s = A[k,i] - sum_{p=0..k-1} U[p,k] * U[p,i]
                double s = Aflat[k * N + i]; // A[k,i]
                for (int p = 0; p < k; ++p) {
                    double upk = Uflat[p * N + k];
                    double upi = Uflat[p * N + i];
                    s -= upk * upi;
                }
                Uflat[k * N + i] = s / akk;
            }
        }

        pthread_mutex_lock(&mutex_shared);
        ++finishedCount;
        if (finishedCount >= numThreads) {
            pthread_cond_signal(&cvDone);
        }
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
    // optionally allow passing number of threads as second argument
    if (argc >= 3) {
        int t = atoi(argv[2]);
        if (t > 0) numThreads = t;
    }

    try {
        ReadMatrixAndVector(folder, Aflat, B, N);
    } catch (const std::exception& ex) {
        std::cerr << "Ошибка при чтении входных файлов: " << ex.what() << "\n";
        return 1;
    }

    // allocate Uflat and zero it
    Uflat.assign((size_t)N * N, 0.0);

    workers.resize(numThreads);
    threads.resize(numThreads);
    int indicesPerThread = (N + numThreads - 1) / numThreads;
    for (int t = 0; t < numThreads; ++t) {
        int s = t * indicesPerThread;
        int e = std::min((t + 1) * indicesPerThread, N);
        workers[t].id = t;
        workers[t].startIdx = s;
        workers[t].endIdx = e;
    }

    // create threads
    for (int t = 0; t < numThreads; ++t) {
        if (pthread_create(&threads[t], nullptr, WorkerRoutine, &workers[t]) != 0) {
            std::cerr << "Ошибка при создании потока " << t << "\n";
            return 2;
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();

    // Main Cholesky loop
    for (int k = 0; k < N; ++k) {
        // compute diagonal U[k,k] = sqrt(A[k,k] - sum_{p<k} U[p,k]^2)
        double sum_diag = Aflat[k * N + k];
        for (int p = 0; p < k; ++p) {
            double upk = Uflat[p * N + k];
            sum_diag -= upk * upk;
        }
        if (sum_diag <= 0.0) {
            // signal termination and join threads gracefully
            pthread_mutex_lock(&mutex_shared);
            terminateFlag = true;
            pthread_cond_broadcast(&cvStart);
            pthread_mutex_unlock(&mutex_shared);
            for (int t = 0; t < numThreads; ++t) pthread_join(threads[t], nullptr);
            std::cerr << "Матрица не является положительно определенной\n";
            return 1;
        }
        double akk = std::sqrt(sum_diag);
        Uflat[k * N + k] = akk;

        // set shared_k and akk, reset finishedCount, wake workers
        pthread_mutex_lock(&mutex_shared);
        current_k = k;
        shared_akk = akk;
        finishedCount = 0;
        pthread_cond_broadcast(&cvStart);

        // wait until all workers finished
        while (finishedCount < numThreads) {
            pthread_cond_wait(&cvDone, &mutex_shared);
        }
        current_k = -1;
        pthread_mutex_unlock(&mutex_shared);
    }

    // terminate workers
    pthread_mutex_lock(&mutex_shared);
    terminateFlag = true;
    pthread_cond_broadcast(&cvStart);
    pthread_mutex_unlock(&mutex_shared);

    for (int t = 0; t < numThreads; ++t) pthread_join(threads[t], nullptr);

    // Solve U^T * Y = B (forward)
    std::vector<double> Y(N);
    for (int i = 0; i < N; ++i) {
        double s = B[i];
        for (int p = 0; p < i; ++p) {
            s -= Uflat[p * N + i] * Y[p];
        }
        Y[i] = s / Uflat[i * N + i];
    }

    // Solve U * X = Y (backward)
    std::vector<double> X(N);
    for (int i = N - 1; i >= 0; --i) {
        double s = Y[i];
        for (int j = i + 1; j < N; ++j)
            s -= Uflat[i * N + j] * X[j];
        X[i] = s / Uflat[i * N + i];
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

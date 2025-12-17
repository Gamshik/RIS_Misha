#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <pthread.h>
#include <unistd.h> // sysconf
#include <clocale>  // setlocale

// ==========================================================
// Эмуляция AutoResetEvent средствами POSIX
// В Windows Event - это системный объект. В Linux мы делаем его сами.
class AutoResetEvent {
private:
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool signaled;

public:
    AutoResetEvent() {
        pthread_mutex_init(&mutex, nullptr);
        pthread_cond_init(&cond, nullptr);
        signaled = false;
    }

    ~AutoResetEvent() {
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&cond);
    }

    // Аналог WaitForSingleObject
    void Wait() {
        pthread_mutex_lock(&mutex);
        while (!signaled) {
            pthread_cond_wait(&cond, &mutex);
        }
        signaled = false; // Auto-reset: сбрасываем флаг после пробуждения
        pthread_mutex_unlock(&mutex);
    }

    // Аналог SetEvent
    void Signal() {
        pthread_mutex_lock(&mutex);
        signaled = true;
        pthread_cond_signal(&cond); // Будим одного ждущего
        pthread_mutex_unlock(&mutex);
    }
};

// ==========================================================
// Данные потока
struct ThreadData {
    int threadId;
    int startCol;
    int endCol;
    int N;

    double* Aflat;

    std::vector<std::vector<double>>* Ldiag;
    std::vector<std::vector<double>>* Udiag;

    volatile int currentI;
    
    // Указатели на события синхронизации
    AutoResetEvent* startEvent;
    AutoResetEvent* doneEvent;
    
    volatile bool terminate;
};

// ==========================================================
// Доступ к диагоналям (аналогично Windows версии)
inline double Uat(const std::vector<std::vector<double>>& U, int i, int j) {
    return U[j - i][i]; 
}
inline void Uset(std::vector<std::vector<double>>& U, int i, int j, double v) {
    U[j - i][i] = v;
}

inline double Lat(const std::vector<std::vector<double>>& L, int i, int j) {
    return (i == j) ? 1.0 : L[i - j][j];
}
inline void Lset(std::vector<std::vector<double>>& L, int i, int j, double v) {
    L[i - j][j] = v;
}

// ==========================================================
// Функция потока (Pthreads signature: void* -> void*)
void* LUWorker(void* lpParam) {
    ThreadData* data = (ThreadData*)lpParam;
    int N = data->N;

    while (true) {
        // Ждем команды старта от главного потока
        data->startEvent->Wait();

        if (data->terminate) break;

        int i = data->currentI;

        // ---------- U[i,j], j >= i ----------
        for (int j = std::max(i, data->startCol); j < data->endCol && j < N; ++j) {
            double sum = 0.0;
            for (int k = 0; k < i; ++k)
                sum += Lat(*data->Ldiag, i, k) * Uat(*data->Udiag, k, j);

            Uset(*data->Udiag, i, j, data->Aflat[i * N + j] - sum);
        }

        // ---------- L[j,i], j > i ----------
        for (int j = std::max(i + 1, data->startCol); j < data->endCol && j < N; ++j) {
            double sum = 0.0;
            for (int k = 0; k < i; ++k)
                sum += Lat(*data->Ldiag, j, k) * Uat(*data->Udiag, k, i);

            Lset(*data->Ldiag, j, i,
                 (data->Aflat[j * N + i] - sum) / Uat(*data->Udiag, i, i));
        }

        // Сообщаем, что закончили итерацию
        data->doneEvent->Signal();
    }
    return nullptr;
}

// ==========================================================
// Чтение матрицы (пути исправлены на Linux-style)
bool ReadMatrix(const std::string& folder,
                std::vector<double>& A,
                std::vector<double>& B,
                int& N) 
{
    // В Linux разделитель пути '/'
    std::ifstream fa(folder + "/A.txt");
    if (!fa.is_open()) return false;

    std::vector<std::vector<double>> tmp;
    std::string line;
    while (std::getline(fa, line)) {
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

    std::ifstream fb(folder + "/B.txt");
    if (!fb.is_open()) return false;

    B.resize(N);
    for (int i = 0; i < N; ++i) fb >> B[i];
    fb.close();

    return true;
}

// ==========================================================
// Запись результата
void WriteVector(const std::string& path, const std::vector<double>& X) {
    std::ofstream f(path);
    f << std::setprecision(17);
    for (double v : X) f << v << "\n";
    f.close();
}

// ==========================================================
// Main
int main(int argc, char* argv[]) {
    // Включаем поддержку кириллицы в консоли Linux
    std::setlocale(LC_ALL, "");

    if (argc < 2) {
        std::wcout << L"Ожидался аргумент — путь к папке с данными." << std::endl;
        return 1;
    }

    std::string folder = argv[1];

    // Определение количества ядер в Linux
    int numThreads = sysconf(_SC_NPROCESSORS_ONLN);
    if (argc >= 3) {
        int t = atoi(argv[2]);
        if (t > 0) numThreads = t;
    }

    std::vector<double> A, B;
    int N;
    if (!ReadMatrix(folder, A, B, N)) {
        std::wcout << L"Ошибка чтения входных файлов." << std::endl;
        return 1;
    }

    // ---------- диагональное хранение ----------
    std::vector<std::vector<double>> Ldiag(N), Udiag(N);
    for (int d = 0; d < N; ++d) {
        Udiag[d].resize(N - d);
        if (d > 0) Ldiag[d].resize(N - d);
    }

    // Инициализация потоков и примитивов синхронизации
    std::vector<ThreadData> tdata(numThreads);
    std::vector<pthread_t> threads(numThreads);
    
    // Используем векторы указателей, так как AutoResetEvent нельзя копировать/перемещать
    // из-за mutex/cond внутри (либо нужно писать конструктор копирования, но указатели проще)
    std::vector<AutoResetEvent*> startEv(numThreads);
    std::vector<AutoResetEvent*> doneEv(numThreads);

    int colsPerThread = (N + numThreads - 1) / numThreads;

    for (int t = 0; t < numThreads; ++t) {
        int sc = t * colsPerThread;
        int ec = std::min(sc + colsPerThread, N);

        startEv[t] = new AutoResetEvent();
        doneEv[t]  = new AutoResetEvent();

        tdata[t] = { t, sc, ec, N, A.data(), &Ldiag, &Udiag, -1, startEv[t], doneEv[t], false };
        
        // Создание потока
        pthread_create(&threads[t], nullptr, LUWorker, &tdata[t]);
    }

    auto t1 = std::chrono::high_resolution_clock::now();

    // Основной цикл разложения
    for (int i = 0; i < N; ++i) {
        // Устанавливаем текущую строку для всех
        for (int t = 0; t < numThreads; ++t)
            tdata[t].currentI = i;

        // Даем отмашку всем потокам (SetEvent)
        for (int t = 0; t < numThreads; ++t)
            startEv[t]->Signal();

        // Ждем завершения всех потоков на этой итерации
        // (Аналог WaitForMultipleObjects bWaitAll=TRUE)
        for (int t = 0; t < numThreads; ++t)
            doneEv[t]->Wait();
    }

    // ---------- прямой ход ----------
    std::vector<double> Z(N);
    for (int i = 0; i < N; ++i) {
        double sum = B[i];
        for (int j = 0; j < i; ++j)
            sum -= Lat(Ldiag, i, j) * Z[j];
        Z[i] = sum;
    }

    // ---------- обратный ход ----------
    std::vector<double> X(N);
    for (int i = N - 1; i >= 0; --i) {
        double sum = Z[i];
        for (int j = i + 1; j < N; ++j)
            sum -= Uat(Udiag, i, j) * X[j];
        X[i] = sum / Uat(Udiag, i, i);
    }

    auto t2 = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration<double, std::milli>(t2 - t1).count();

    WriteVector(folder + "/X.txt", X);

    std::wcout << L"==============================================" << std::endl;
    std::wcout << L"Размер матрицы: " << N << L"x" << N << std::endl;
    std::wcout << L"Потоков: " << numThreads << std::endl;
    std::wcout << L"Время: " << ms << L" мс" << std::endl;
    std::wcout << L"==============================================" << std::endl;

    // ---------- завершение потоков ----------
    for (int t = 0; t < numThreads; ++t) {
        tdata[t].terminate = true;
        startEv[t]->Signal(); // Будим, чтобы они вышли из цикла
    }
    
    // Ждем физического завершения (pthread_join)
    for (int t = 0; t < numThreads; ++t) {
        pthread_join(threads[t], nullptr);
    }

    // Очистка памяти
    for (int t = 0; t < numThreads; ++t) {
        delete startEv[t];
        delete doneEv[t];
    }

    return 0;
}
#include "globals.h"

double Function(double x) {
    return std::exp(-x) * std::cos(x);
}


DWORD WINAPI ThreadVolumeLeftRect(LPVOID param) {
    ThreadData* d = static_cast<ThreadData*>(param);

    // Шаг разбиения (ширина одного прямоугольника)
    double dx = (d->end - d->start) / d->segments;
    // Локальная сумма для текущего потока
    double localSum = 0.0;

    // Основной цикл метода левых прямоугольников
    for (int i = 0; i < d->segments; i++) {
        // Левая точка i-го подотрезка
        double x = d->start + i * dx;
        // Значение функции f(x)
        double fx = Function(x);        
        // Добавляем объём "цилиндрика":
        // π * [f(x)]² * dx
        localSum += PI * fx * fx * dx;
    }

    // INFO: твоя критическая секция, ждём пока мьютекст освободится если он занят
    WaitForSingleObject(g_mutex, INFINITE);
    g_globalResult += localSum;
    // INFO: когда мы мьютекста дождались, мы его сразу заняли в WaitForSingleObject, 
    // теперь его нужно освободить, чтобы у других был доступ к критической секции
    ReleaseMutex(g_mutex);

    return 0;
}

double CalculateVolumeSingleThread(double a, double b, int n) {
    double dx = (b - a) / n;
    double sum = 0.0;

    for (int i = 0; i < n; i++) {
        double x = a + i * dx;
        double fx = Function(x);
        sum += PI * fx * fx * dx;
    }

    return sum;
}

// INFO: сэмп - точка в которой считается значение, поэтому чем больше точность, тем больше семплов
int GetNFromEpsilon(double eps) {
    if (eps >= 0.1) return 200;
    if (eps >= 0.01) return 2000;
    if (eps >= 0.001) return 20000;
    if (eps >= 0.0001) return 200000;
    return 1000000;
}

double CalculateVolume(double a, double b, int n, int threadCount) {
    if (threadCount == 1)
        return CalculateVolumeSingleThread(a, b, n);

    g_globalResult = 0.0;

    // INFO: массив дскрипторов потоков
    std::vector<HANDLE> threads(threadCount);
    std::vector<ThreadData> t(threadCount);

    // INFO: рассчитываем кол-во точек на поток
    int perThread = n / threadCount;
    
    // INFO: оставшиеся
    int remainder = n % threadCount;

    double dx = (b - a) / n;

    double curA = a;

    for (int i = 0; i < threadCount; i++) {
        // INFO: если есть оставшиеся точки, то каждому потоку докидываем по 1 пока не кончатся
        int count = perThread + (i < remainder ? 1 : 0);

        t[i].start = curA;
        t[i].end = curA + count * dx;
        t[i].segments = count;

        curA += count * dx;

        threads[i] = CreateThread(nullptr, 0, ThreadVolumeLeftRect, &t[i], 0, nullptr);
    }

    // INFO: ожидаем все потоки
    WaitForMultipleObjects(threadCount, threads.data(), TRUE, INFINITE);
    for (auto& h : threads) CloseHandle(h);

    return g_globalResult;
}

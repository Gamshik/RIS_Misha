#include "globals.h"

// INFO: тут я не думаю, что нужны объяснения, всё интуитивно понятно, ес чо спросишь,
// тут просто вызываем функции
void RunBenchmarks() {
    g_benchmarkResults.clear();

    double a = g_currentA;
    double b = g_currentB;

    // Тест 1: Зависимость от точности (1 и 3 потока)
    std::vector<double> epsilons = {0.1, 0.01, 0.001, 0.0001, 0.00001};

    // INFO: вот для чего нужны дискрипторы ещё, он у нас объявлен глобально, 
    // и мы ставим при помощи него тексть в блок результатов
    SetWindowTextW(g_hResultText, L"Running benchmarks...\r\n");

    // INFO: заставляет окно ререндериться НЕМЕДЛЕННО
    UpdateWindow(g_hMainWnd);

    for (double eps : epsilons) {
        int samples = GetNFromEpsilon(eps);

        // 1 поток
        auto start = std::chrono::high_resolution_clock::now();
        double surf = CalculateVolume(a, b, samples, 1);
        auto end = std::chrono::high_resolution_clock::now();
        double time = std::chrono::duration<double>(end - start).count();
        g_benchmarkResults.push_back({1, eps, time, surf});

        // 3 потока
        start = std::chrono::high_resolution_clock::now();
        surf = CalculateVolume(a, b, samples, 3);
        end = std::chrono::high_resolution_clock::now();
        time = std::chrono::duration<double>(end - start).count();
        g_benchmarkResults.push_back({3, eps, time, surf});
    }

    // Тест 2: Зависимость от количества потоков (eps = 0.00001)
    double fixedEps = 0.00001;
    int fixedSamples = GetNFromEpsilon(fixedEps);

    for (int threads = 1; threads <= 10; ++threads) {
        auto start = std::chrono::high_resolution_clock::now();
        double surf = CalculateVolume(a, b, fixedSamples, threads);
        auto end = std::chrono::high_resolution_clock::now();
        double time = std::chrono::duration<double>(end - start).count();
        g_benchmarkResults.push_back({threads, fixedEps, time, surf});
    }

    // INFO: условно, помечает твоё окно, как грязное, и что его нужно будет перерендерить при следующем вызове WM_PAINT (смотри в WndProc в main.cpp)
    // Там винда под копотом сама решит когда вызвать WM_PAINT, оптимизация и тд есть какая-то
    // Плюс, ты свои бенчмарки рисуешь в секции WM_PAINT, чтобы они рисовались не на секунду, а постоянно, если они есть, 
    // и если ты тут попытаешься вызвать UpdateWindow, то у тебя просто не отрисуются граффики, так как данные не успеют присвоится мб
    InvalidateRect(g_hMainWnd, nullptr, TRUE);
}

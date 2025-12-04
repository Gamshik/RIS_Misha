#include "globals.h"

// --- Определения глобальных переменных ---
// INFO: просто определяем
HWND g_hMainWnd = nullptr;
HWND g_hEditA = nullptr;
HWND g_hEditB = nullptr;
HWND g_hEditEps = nullptr;
HWND g_hEditThreads = nullptr;
HWND g_hResultText = nullptr;
HINSTANCE g_hInst = nullptr;

std::vector<BenchmarkResult> g_benchmarkResults;
double g_currentA = 0.5;
double g_currentB = PI;
HANDLE g_mutex = nullptr;
double g_globalResult = 0.0;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    g_hInst = hInstance;

    // INFO: создаём наш мьютекс
    g_mutex = CreateMutex(nullptr, FALSE, nullptr);
    if (g_mutex == nullptr) {
        MessageBoxW(nullptr, L"Mutex creation error", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // INFO: создаём класс, который хранит конфиг окна
    WNDCLASSW wc = {};
    // INFO: этот метод обрабатывает события
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"Lab3Window";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);

    if (!RegisterClassW(&wc)) {
        MessageBoxW(nullptr, L"Window class registration error", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // INFO: Создаёт окно
    g_hMainWnd = CreateWindowW(
        L"Lab3Window",
        L"3",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1500, 900,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!g_hMainWnd) {
        MessageBoxW(nullptr, L"Window creation error", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);

    MSG msg;
    // INFO: чтобы получали сообщения
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return static_cast<int>(msg.wParam);
}

// INFO: обрабатываем сообщения
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE:
            CreateControls(hwnd);
            break;

        // INFO: сообщения от кнопок
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_BUTTON_CALC) { 
                // INFO: собираем данные с инпутов
                wchar_t buffer[256];
                GetWindowTextW(g_hEditA, buffer, 256);
                double a = _wtof(buffer);
                GetWindowTextW(g_hEditB, buffer, 256);
                double b = _wtof(buffer);
                GetWindowTextW(g_hEditEps, buffer, 256);
                double eps = _wtof(buffer);
                GetWindowTextW(g_hEditThreads, buffer, 256);
                int threads = _wtoi(buffer);

                if (threads < 1) threads = 1;
                if (threads > MAX_THREADS) threads = MAX_THREADS;
                if (std::abs(a) < 1e-10) a = 0.5; // Избегаем нуля

                g_currentA = a;
                g_currentB = b;

                int samples = GetNFromEpsilon(eps);

                // Однопоточное вычисление
                auto start = std::chrono::high_resolution_clock::now();
                double surfaceSingle = CalculateVolumeSingleThread(a, b, samples);
                auto endSingle = std::chrono::high_resolution_clock::now();
                double timeSingle = std::chrono::duration<double>(endSingle - start).count();

                // Многопоточное вычисление
                start = std::chrono::high_resolution_clock::now();
                double surface = CalculateVolume(a, b, samples, threads);
                auto end = std::chrono::high_resolution_clock::now();
                double time = std::chrono::duration<double>(end - start).count();

                std::wostringstream result;
                result << std::fixed << std::setprecision(6);
                result << L"=== CALCULATION RESULTS ===\r\n\r\n";
                result << L"Function: f(x) = e^(-x) * cos(x)\r\n";
                result << L"Interval: [" << a << L"; " << b << L"]\r\n";
                result << L"Precision (epsilon): " << eps << L"\r\n";
                result << L"Single-threaded calculation:\r\n";
                result << L"  Surface: " << surfaceSingle << L"\r\n";
                result << L"  Time: " << timeSingle << L" sec\r\n\r\n";
                result << L"Multi-threaded calculation (" << threads << L" threads):\r\n";
                result << L"  Surface: " << surface << L"\r\n";
                result << L"  Time: " << time << L" sec\r\n";

                if (time > 0) {
                    result << L"\r\n";
                    result << L"  Speedup: " << (timeSingle / time) << L"x\r\n";
                }

                // INFO: пишем текст в ридонли поле
                SetWindowTextW(g_hResultText, result.str().c_str());
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            else if (LOWORD(wParam) == IDC_BUTTON_BENCH) { 
                SetWindowTextW(g_hResultText, L"Running benchmarks...");
                UpdateWindow(hwnd);

                RunBenchmarks();

                std::wostringstream result;
                result << L"=== BENCHMARK RESULTS ===\r\n\r\n";
                result << L"Completed tests\r\n";
                result << L"Graphs are displayed\r\n";

                SetWindowTextW(g_hResultText, result.str().c_str());
            }
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT graphRect = {GRAPH_LEFT, GRAPH_TOP, GRAPH_LEFT + GRAPH_WIDTH, GRAPH_TOP + GRAPH_HEIGHT};
            DrawGraph(hdc, graphRect);

            // INFO: если есть данные с бенчамрков, то рисуем графики 
            if (!g_benchmarkResults.empty()) {
                RECT benchRect = {BENCH_GRAPH_LEFT, BENCH_GRAPH_TOP,
                                 BENCH_GRAPH_LEFT + BENCH_GRAPH_WIDTH, BENCH_GRAPH_TOP + BENCH_GRAPH_HEIGHT};
                DrawBenchmarkGraphs(hdc, benchRect);
            }

            EndPaint(hwnd, &ps);
            break;
        }

        // INFO: ну тут понятно, просто очищаем
        case WM_DESTROY:
            if (g_mutex) CloseHandle(g_mutex);
            PostQuitMessage(0);
            break;

        // INFO: другие сообщение, которые сама винда может присылать, чтобы корректно всё было
        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

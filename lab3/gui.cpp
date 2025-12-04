
#include "globals.h"

// INFO: это вообще ему можешь не показывать, я ему даж не говорил про это, 
// но для себя если хош, я кратко внизу опишу, тут в целом всё интуитивно понятно

// INFO: метод рисует контролсы - поля + кнопки в левом верхнем углу
void CreateControls(HWND hwnd) {
    // INFO: локальные переменные
    int startX = 20;
    int startY = 20;
    int labelWidth = 140;
    int editWidth = 100;
    int colGap = 250; // Расстояние между колонками
    int rowGap = 35;

    // --- Левая колонка ---
    int yPos = startY;

    // INFO: просто статической окно - лейбл твоего инпута, 
    // в целом наводишься на функцию и поймёшь за что параметры отвечают,
    // иначе тут придётся написать целый рассказ 
    CreateWindowW(L"STATIC", L"Start point A:", WS_VISIBLE | WS_CHILD,
        startX, yPos, labelWidth, 25, hwnd, nullptr, g_hInst, nullptr);
    // INFO: а это уже твой инпут. Крч, есть такая штука как дескриптор - просто указатель в памяти на какой-либо объект,
    // в данном случае g_hEditA - это дескриптор указывающий на окно для ввода определённых данных,
    // нам это нужно, чтобы потом читать введённые данные при помощи этого дескриптора. Его объявление находится в globals.h
    g_hEditA = CreateWindowW(L"EDIT", L"0.5", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT,
        startX + labelWidth, yPos, editWidth, 25, hwnd, nullptr, g_hInst, nullptr);
    
    // INFO: отступ между строками
    yPos += rowGap;

    // INFO: дальше всё по аналогиии

    CreateWindowW(L"STATIC", L"End point B:", WS_VISIBLE | WS_CHILD,
        startX, yPos, labelWidth, 25, hwnd, nullptr, g_hInst, nullptr);
    g_hEditB = CreateWindowW(L"EDIT", L"3.14159", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT,
        startX + labelWidth, yPos, editWidth, 25, hwnd, nullptr, g_hInst, nullptr);
    yPos += rowGap;

    // --- Правая колонка ---
    int yPosRight = startY;
    int rightX = startX + colGap;
    CreateWindowW(L"STATIC", L"Precision (epsilon):", WS_VISIBLE | WS_CHILD,
        rightX, yPosRight, labelWidth + 20, 25, hwnd, nullptr, g_hInst, nullptr);
    g_hEditEps = CreateWindowW(L"EDIT", L"0.00001", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT,
        rightX + labelWidth + 20, yPosRight, editWidth, 25, hwnd, nullptr, g_hInst, nullptr);
    yPosRight += rowGap;

    CreateWindowW(L"STATIC", L"Number of threads:", WS_VISIBLE | WS_CHILD,
        rightX, yPosRight, labelWidth + 20, 25, hwnd, nullptr, g_hInst, nullptr);
    g_hEditThreads = CreateWindowW(L"EDIT", L"4", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT,
        rightX + labelWidth + 20, yPosRight, editWidth, 25, hwnd, nullptr, g_hInst, nullptr);
    yPosRight += rowGap + 10;

    // --- Кнопки ---
    // INFO: в каждую кнопку в 3 с конца параметр ты передаёшь просто число, которое будет отправляться программе при нажатии,
    // в твоём случае - это IDC_BUTTON_CALC и IDC_BUTTON_BENCH, обрабатываешь ты их в WndProc в main.cpp
    CreateWindowW(L"BUTTON", L"Calculate Surface",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_FLAT,
        startX, std::max(yPos, yPosRight), 180, 40, hwnd, (HMENU)IDC_BUTTON_CALC, g_hInst, nullptr);
    CreateWindowW(L"BUTTON", L"Run Benchmarks",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_FLAT,
        startX + 200, std::max(yPos, yPosRight), 180, 40, hwnd, (HMENU)IDC_BUTTON_BENCH, g_hInst, nullptr);

    // --- Результаты ---
    // INFO: readonly поле для записи результатов
    g_hResultText = CreateWindowW(L"EDIT", L"",
        WS_VISIBLE | WS_CHILD | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        startX, std::max(yPos, yPosRight) + 60, 550, 350, hwnd, nullptr, g_hInst, nullptr);
}

// INFO: hdc - от куда будет рисовать
void DrawGraph(HDC hdc, RECT rect) {
    // Рамка: тёмно-серый
    // INFO: чтобы рисовать линии, нужно перо, и ты его создаёшт каждый раз как нужно 
    HPEN framePen = CreatePen(PS_SOLID, 2, RGB(80, 80, 80));
    SelectObject(hdc, framePen);
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    int centerX = width / 2;
    int centerY = height / 2;

    // Сетка: светло-голубая пунктирная
    HPEN gridPen = CreatePen(PS_DOT, 1, RGB(173, 216, 230)); // LightBlue
    SelectObject(hdc, gridPen);
    for (int i = 50; i < width; i += 50) {
        MoveToEx(hdc, rect.left + i, rect.top, nullptr);
        LineTo(hdc, rect.left + i, rect.bottom);
    }
    for (int i = 50; i < height; i += 50) {
        MoveToEx(hdc, rect.left, rect.top + i, nullptr);
        LineTo(hdc, rect.right, rect.top + i);
    }

    // Оси координат: ярко-синие с чуть большей толщиной
    HPEN axisPen = CreatePen(PS_SOLID, 3, RGB(30, 144, 255)); // DodgerBlue
    SelectObject(hdc, axisPen);
    MoveToEx(hdc, rect.left, rect.top + centerY, nullptr);
    LineTo(hdc, rect.right, rect.top + centerY);
    MoveToEx(hdc, rect.left + centerX, rect.top, nullptr);
    LineTo(hdc, rect.left + centerX, rect.bottom);

    // График функции: ярко-оранжевый
    HPEN graphPen = CreatePen(PS_SOLID, 3, RGB(255, 140, 0)); // DarkOrange
    SelectObject(hdc, graphPen);

    double xMin = g_currentA;
    double xMax = g_currentB;
    double xRange = xMax - xMin;

    double yMin = 1e10, yMax = -1e10;
    for (int i = 0; i < width; ++i) {
        double x = xMin + (i * xRange) / width;
        double y = Function(x);
        yMin = std::min(yMin, y);
        yMax = std::max(yMax, y);
    }

    double yRange = yMax - yMin;
    if (yRange < 1e-10) yRange = 1.0;

    bool firstPoint = true;
    for (int i = 0; i < width; ++i) {
        double x = xMin + (i * xRange) / width;
        double y = Function(x);
        int screenX = rect.left + i;
        int screenY = rect.top + height - static_cast<int>(((y - yMin) / yRange) * height * 0.9) - height * 0.05;

        if (screenY < rect.top) screenY = rect.top;
        if (screenY > rect.bottom) screenY = rect.bottom;

        if (firstPoint) {
            MoveToEx(hdc, screenX, screenY, nullptr);
            firstPoint = false;
        } else {
            LineTo(hdc, screenX, screenY);
        }
    }

    // Заголовок: тёмно-синий жирный шрифт
    SetTextColor(hdc, RGB(0, 0, 139)); // DarkBlue
    SetBkMode(hdc, TRANSPARENT);
    TextOutW(hdc, rect.left + 10, rect.top + 10, L"f(x) = e^(-x) * cos(x)", 23);

    DeleteObject(framePen);
    DeleteObject(gridPen);
    DeleteObject(axisPen);
    DeleteObject(graphPen);
}

void DrawBenchmarkGraphs(HDC hdc, RECT rect) {
    if (g_benchmarkResults.empty()) return;

    // Расширяем области графиков, делаем больше пространства
    int padding = 20;
    int halfWidth = (rect.right - rect.left) / 2;
    RECT leftRect  = {rect.left + padding, rect.top + padding, rect.left + halfWidth - padding, rect.bottom - padding};
    RECT rightRect = {rect.left + halfWidth + padding, rect.top + padding, rect.right - padding, rect.bottom - padding};

    // ------------------- График 1: Time vs Precision -------------------
    std::vector<BenchmarkResult> epsResults;
    for (const auto& r : g_benchmarkResults) {
        if ((r.threadCount == 1 || r.threadCount == 3) && r.epsilon >= 0.00001) {
            epsResults.push_back(r);
        }
    }

    if (!epsResults.empty()) {
        std::vector<double> epsValues;
        for (const auto& r : epsResults) epsValues.push_back(r.epsilon);
        std::sort(epsValues.begin(), epsValues.end(), std::greater<double>());
        epsValues.erase(std::unique(epsValues.begin(), epsValues.end()), epsValues.end());

        std::vector<double> times1(epsValues.size(), 0.0);
        std::vector<double> times3(epsValues.size(), 0.0);
        for (size_t i = 0; i < epsValues.size(); ++i) {
            double eps = epsValues[i];
            for (const auto& r : epsResults) {
                if (std::abs(r.epsilon - eps) < 1e-15) {
                    if (r.threadCount == 1) times1[i] = r.time;
                    if (r.threadCount == 3) times3[i] = r.time;
                }
            }
        }

        // Заголовок графика
        SetTextColor(hdc, RGB(0, 51, 102)); // тёмно-синий
        SetBkMode(hdc, TRANSPARENT);
        HFONT hFont = CreateFontW(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        SelectObject(hdc, hFont);
        TextOutW(hdc, leftRect.left + 100, leftRect.top, L"Time vs Precision", 17);
        DeleteObject(hFont);

        int plotLeft = leftRect.left + 50;
        int plotTop = leftRect.top + 40;
        int plotRight = leftRect.right - 20;
        int plotBottom = leftRect.bottom - 30;

        int plotW = plotRight - plotLeft;
        int plotH = plotBottom - plotTop;

        // Фон графика: светло-серый
        HBRUSH plotBg = CreateSolidBrush(RGB(245, 245, 250));
        RECT plotArea = {plotLeft, plotTop, plotRight, plotBottom};
        FillRect(hdc, &plotArea, plotBg);
        DeleteObject(plotBg);
        Rectangle(hdc, plotLeft, plotTop, plotRight, plotBottom);

        double maxTime = 0.0;
        for (double t : times1) maxTime = std::max(maxTime, t);
        for (double t : times3) maxTime = std::max(maxTime, t);
        if (maxTime <= 0.0) maxTime = 1.0;

        int n = static_cast<int>(epsValues.size());

        // Оси и подписи
        HPEN axisPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
        SelectObject(hdc, axisPen);

        for (int i = 0; i <= n - 1; ++i) {
            int x = plotLeft + (i * plotW) / std::max(1, n - 1);
            MoveToEx(hdc, x, plotBottom, nullptr);
            LineTo(hdc, x, plotBottom + 5);

            wchar_t buf[64];
            swprintf_s(buf, L"%.0e", epsValues[i]);
            TextOutW(hdc, x - 20, plotBottom + 8, buf, static_cast<int>(wcslen(buf)));
        }

        int stepsY = 5;
        for (int i = 0; i <= stepsY; ++i) {
            int y = plotBottom - (i * plotH) / stepsY;
            MoveToEx(hdc, plotLeft - 5, y, nullptr);
            LineTo(hdc, plotLeft, y);

            double val = (i * maxTime) / stepsY;
            wchar_t buf[64];
            swprintf_s(buf, L"%.3f", val);
            TextOutW(hdc, plotLeft - 55, y - 8, buf, static_cast<int>(wcslen(buf)));
        }

        TextOutW(hdc, (plotLeft + plotRight)/2 - 40, plotBottom + 30, L"Precision (ε)", 14);
        TextOutW(hdc, plotLeft - 50, plotTop - 25, L"Time (s)", 8);

        // Графики линий
        HPEN pen1 = CreatePen(PS_SOLID, 3, RGB(0, 102, 204)); // синий
        HPEN pen3 = CreatePen(PS_SOLID, 3, RGB(204, 51, 0));  // красный

        SelectObject(hdc, pen1);
        for (int i = 0; i < n; ++i) {
            int x = plotLeft + (i * plotW) / std::max(1, n - 1);
            int y = plotBottom - static_cast<int>((times1[i] / maxTime) * plotH);
            if (i == 0) MoveToEx(hdc, x, y, nullptr);
            else LineTo(hdc, x, y);
            Ellipse(hdc, x - 4, y - 4, x + 4, y + 4);
        }

        SelectObject(hdc, pen3);
        for (int i = 0; i < n; ++i) {
            int x = plotLeft + (i * plotW) / std::max(1, n - 1);
            int y = plotBottom - static_cast<int>((times3[i] / maxTime) * plotH);
            if (i == 0) MoveToEx(hdc, x, y, nullptr);
            else LineTo(hdc, x, y);
            Ellipse(hdc, x - 4, y - 4, x + 4, y + 4);
        }

        RECT legend = {plotLeft + 30, plotTop + 10, plotLeft + 150, plotTop + 60};
        HBRUSH lbg = CreateSolidBrush(RGB(245,245,245));
        FillRect(hdc, &legend, lbg);
        DeleteObject(lbg);
        Rectangle(hdc, legend.left, legend.top, legend.right, legend.bottom);

        HPEN legendPen1 = CreatePen(PS_SOLID, 3, RGB(0,0,200));
        HPEN legendPen3 = CreatePen(PS_SOLID, 3, RGB(200,0,0));
        SelectObject(hdc, legendPen1);
        MoveToEx(hdc, legend.left + 10, legend.top + 15, nullptr);
        LineTo(hdc, legend.left + 40, legend.top + 15);
        SelectObject(hdc, legendPen3);
        MoveToEx(hdc, legend.left + 10, legend.top + 35, nullptr);
        LineTo(hdc, legend.left + 40, legend.top + 35);
        SetTextColor(hdc, RGB(0,0,0));
        TextOutW(hdc, legend.left + 45, legend.top + 7, L"1 thread", 8);
        TextOutW(hdc, legend.left + 45, legend.top + 27, L"3 threads", 9);


        DeleteObject(pen1);
        DeleteObject(pen3);
        DeleteObject(axisPen);
        DeleteObject(legendPen1);
        DeleteObject(legendPen3);
    }

    // ------------------- График 2: Time vs Threads -------------------
    std::vector<BenchmarkResult> threadResults;
    int skip = 2;

    for (const auto& r : g_benchmarkResults) {
        if (std::abs(r.epsilon - 0.00001) < 1e-12 &&
            r.threadCount >= 1 && r.threadCount <= 10)
        {
            if (skip > 0) { skip--; continue; }
            threadResults.push_back(r);
        }
    }

    if (!threadResults.empty()) {
        // Заголовок
        HFONT hFont = CreateFontW(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        SelectObject(hdc, hFont);
        SetTextColor(hdc, RGB(0, 51, 102));
        SetBkMode(hdc, TRANSPARENT);
        TextOutW(hdc, rightRect.left + 100, rightRect.top, L"Time vs Threads", 15);
        DeleteObject(hFont);

        int plotLeft = rightRect.left + 50;
        int plotTop = rightRect.top + 40;
        int plotRight = rightRect.right - 20;
        int plotBottom = rightRect.bottom - 30;

        int plotW = plotRight - plotLeft;
        int plotH = plotBottom - plotTop;

        HBRUSH plotBg = CreateSolidBrush(RGB(245, 245, 250));
        RECT plotArea = {plotLeft, plotTop, plotRight, plotBottom};
        FillRect(hdc, &plotArea, plotBg);
        DeleteObject(plotBg);
        Rectangle(hdc, plotLeft, plotTop, plotRight, plotBottom);

        double maxTime = 0.0;
        for (const auto& r : threadResults) maxTime = std::max(maxTime, r.time);

        HPEN threadPen = CreatePen(PS_SOLID, 3, RGB(0, 153, 0)); // зелёный
        SelectObject(hdc, threadPen);

        size_t n = threadResults.size();
        for (size_t i = 0; i < n; ++i) {
            int x = plotLeft + (i * plotW) / std::max<size_t>(1, n - 1);
            int y = plotBottom - static_cast<int>((threadResults[i].time / maxTime) * plotH);
            if (i == 0) MoveToEx(hdc, x, y, nullptr);
            else LineTo(hdc, x, y);
            Ellipse(hdc, x - 4, y - 4, x + 4, y + 4);

            // подписи X
            wchar_t buf[16];
            swprintf_s(buf, L"%d", threadResults[i].threadCount);
            TextOutW(hdc, x - 10, plotBottom + 8, buf, static_cast<int>(wcslen(buf)));
        }

        // оси Y
        int stepsY = 5;
        for (int i = 0; i <= stepsY; ++i) {
            int y = plotBottom - (i * plotH) / stepsY;
            MoveToEx(hdc, plotLeft - 5, y, nullptr);
            LineTo(hdc, plotLeft, y);
            double val = (i * maxTime) / stepsY;
            wchar_t buf[32];
            swprintf_s(buf, L"%.3f", val);
            TextOutW(hdc, plotLeft - 55, y - 8, buf, static_cast<int>(wcslen(buf)));
        }

        TextOutW(hdc, (plotLeft + plotRight)/2 - 50, plotBottom + 30, L"Threads count", 13);
        TextOutW(hdc, plotLeft - 50, plotTop - 25, L"Time (s)", 8);

        RECT legend = {plotRight - 120, plotTop + 10, plotRight - 10, plotTop + 40};
        HBRUSH lbg = CreateSolidBrush(RGB(245,245,245));
        FillRect(hdc, &legend, lbg);
        DeleteObject(lbg);
        Rectangle(hdc, legend.left, legend.top, legend.right, legend.bottom);
        MoveToEx(hdc, legend.left + 10, legend.top + 15, nullptr);
        LineTo(hdc, legend.left + 40, legend.top + 15);
        TextOutW(hdc, legend.left + 45, legend.top + 7, L"ε = 1e-5", 9);

        DeleteObject(threadPen);
    }
}

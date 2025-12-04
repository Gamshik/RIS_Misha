#pragma once
#define NOMINMAX

#include <windows.h>
#include <commctrl.h>
#include <cmath>
#include <vector>
#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <random>


// --- Константы ---
constexpr double PI = 3.14159265358979323846;
constexpr int MAX_THREADS = 10;
constexpr int GRAPH_TOP = 20;
constexpr int GRAPH_LEFT = 600;
constexpr int GRAPH_WIDTH = 600;
constexpr int GRAPH_HEIGHT = 400;

constexpr int BENCH_GRAPH_TOP = 430;
constexpr int BENCH_GRAPH_LEFT = 600;
constexpr int BENCH_GRAPH_WIDTH = 800;
constexpr int BENCH_GRAPH_HEIGHT = 350;


// --- Идентификаторы элементов управления ---
// INFO: вот это айди твоих батонов, нужны для реагоирования на нажатие, всё описал в gui.cpp и main.cpp
#define IDC_BUTTON_CALC 1
#define IDC_BUTTON_BENCH 2


// --- Структуры данных ---
struct ThreadData {
double start;
double end;
int segments;
};


struct BenchmarkResult {
int threadCount;
double epsilon;
double time;
double surface;
};


// --- Глобальные переменные ---
// INFO: это дескрипторы, описал что это в gui.cpp, метод - CreateControls
extern HWND g_hMainWnd;
extern HWND g_hEditA;
extern HWND g_hEditB;
extern HWND g_hEditEps;
extern HWND g_hEditThreads;
extern HWND g_hResultText;
extern HINSTANCE g_hInst;


extern std::vector<BenchmarkResult> g_benchmarkResults;
extern double g_currentA;
extern double g_currentB;
extern HANDLE g_mutex;
extern double g_globalResult;


// --- Прототипы функций ---

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

void CreateControls(HWND hwnd);
void DrawGraph(HDC hdc, RECT rect);
void DrawBenchmarkGraphs(HDC hdc, RECT rect);

double Function(double x);
DWORD WINAPI ThreadVolumeLeftRect(LPVOID param);
double CalculateVolumeSingleThread(double a, double b, int n);
double CalculateVolume(double a, double b, int n, int threadCount);
int GetNFromEpsilon(double eps);

void RunBenchmarks();


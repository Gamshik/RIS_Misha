#pragma once
// INFO: крч, я не совсем шарю что это за дефайны, но они нужны, 
// так как без них у меня была какая-то битва библиотек, 
// видимо у каких-то были подобные дефайны байта, и у меня компилер ругался
#define WIN32_LEAN_AND_MEAN
#include <cstddef>   
#ifdef byte
#undef byte
#endif
// INFO: вот до сюда я не шарю чё это
#include <windows.h>
#include "point3d.h"
#include <vector>
#include <atomic>

using namespace std;

DWORD WINAPI worker_strict_turn(LPVOID lpParam);
void run_single_threaded(const std::vector<Point3D>& points);
void run_multi_thread(const std::vector<Point3D>& points, int num_threads);

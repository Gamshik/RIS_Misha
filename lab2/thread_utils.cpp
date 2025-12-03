#include "thread_utils.h"
#include "math_utils.h"
#include "file_utils.h"
#include <windows.h>
#include <vector>
#include <iostream>
#include <chrono>

DWORD WINAPI worker_strict_turn(LPVOID lpParam) {
    // INFO: парсим полученные в поток данные
    ThreadData* data = (ThreadData*)lpParam;
    const vector<Point3D>& points = *data->points;
    vector<TriangleResult>& shared_results = *data->shared_results;
    atomic<int>* turn = data->turn;
    int thread_id = data->thread_id;
    int num_threads = data->num_threads;
    int n = (int)points.size();

    // INFO: каждый поток начинает от своего индекса и идёт с шагом N - коо-во потоков.
    // например, если у тебя 4 потока:
    // 0-ой будет начинать обрабатывать такие строки - 0 4 8 12 ...
    // 1-ый будет начинать обрабатывать такие строки - 1 5 9 13 ...
    // 2-ой будет начинать обрабатывать такие строки - 2 6 10 14 ...
    // 3-ий будет начинать обрабатывать такие строки - 3 7 11 15 ...
    // INFO: до n-2 потому что дальше идут два цикла, которые берут следующий и через один точки
    for (int i = thread_id; i < n-2; i += num_threads) { 
        vector<TriangleResult> local_results;
        for (int j = i+1; j < n-1; ++j) // INFO: цикл для следующей точки
            for (int k = j+1; k < n; ++k) // INFO: цикл для точки через один
                process_triangle({points[i], points[j], points[k]}, local_results);

        // INFO: тут каждый поток ждёт своей очереди
        while (turn->load() != thread_id) {
            // INFO: это чтобы не было бесконечного цикла без остановы,
            // иначе будет кушать много цпюшки
            Sleep(1); 
        }

        // INFO: бро, вот тут у тебя критическая секция, обозначу.
        // Типо доступ к общей переменной, пон?
        // INFO: ВОТ ЭТО ХОЧЕТ УВИДЕТЬ БАШИК, ТО КАК ТЫ РЕАЛИЗОВАЛ МЕХАНИЗМ СИНХРОНИЗАЦИИ

        // ---------- КРИТИЧЕСКАЯ СЕКЦИЯ ----------

        shared_results.insert(shared_results.end(), local_results.begin(), local_results.end());

        // ---------- КРИТИЧЕСКАЯ СЕКЦИЯ ----------

        // INFO: тут назначаем следующий поток (логику метода синхронизации объяснил в файле info)
        int next = (thread_id + 1) % num_threads;

        turn->store(next);
    }

    return 0;
}

void run_single_threaded(const vector<Point3D>& points) {
    auto start = chrono::high_resolution_clock::now();

    vector<TriangleResult> results;
    size_t n = points.size();

    // INFO: проходим все точки в лоб
    for (size_t i = 0; i < n; ++i)
        for (size_t j = i + 1; j < n; ++j)
            for (size_t k = j + 1; k < n; ++k)
                process_triangle({points[i], points[j], points[k]}, results);

    // INFO: тут считаем время выполнения именно алгоритма,
    // без учёта I/O операций, так как на плюсах они почему-то долго выполняются
    // да и к тому же, у тебя итоговый файл больше гига для 500 точек
    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> duration = end - start;

    write_results_to_file("single_thread_results.txt", results);

    cout << "Acute triangles found: " << results.size() << endl;
    cout << "Triangles search time: " << duration.count() << " seconds" << endl;
}


void run_multi_thread(const vector<Point3D>& points, int num_threads) {
    auto start = chrono::high_resolution_clock::now();

    vector<TriangleResult> shared_results;
    vector<HANDLE> threads(num_threads);
    vector<ThreadData> thread_data(num_threads);
    std::atomic<int> turn_atomic(0);

    // INFO: запускаем потоки
    for (int i = 0; i < num_threads; ++i) {
        thread_data[i].points = &points;
        thread_data[i].shared_results = &shared_results;
        thread_data[i].turn = &turn_atomic;
        thread_data[i].thread_id = i;
        thread_data[i].num_threads = num_threads;

        // INFO: ВОТ ЭТО ХОЧЕТ УВИДЕТЬ БАШИК, ТО КАК ТЫ СОЗДАЁШЬ ПОТОКИ
        threads[i] = CreateThread(
            nullptr,
            0,
            worker_strict_turn,
            &thread_data[i],
            0,
            nullptr
        );
    }

    WaitForMultipleObjects(num_threads, threads.data(), TRUE, INFINITE);

    for (int i = 0; i < num_threads; ++i) CloseHandle(threads[i]);
    
    // INFO: тут считаем время выполнения именно алгоритма,
    // без учёта I/O операций, так как на плюсах они почему-то долго выполняются
    // да и к тому же, у тебя итоговый файл больше гига для 500 точек
    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> duration = end - start;

    write_results_to_file("multi_thread_results.txt", shared_results);

    cout << "Acute triangles found: " << shared_results.size() << "\n";
    cout << "Triangles search time: " << duration.count() << " seconds\n";
}

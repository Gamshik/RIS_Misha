#pragma once
#include <array>
#include <vector>
#include <atomic>

struct Point3D {
    double x, y, z;
};

struct TriangleResult {
    std::array<Point3D, 3> vertices;
    double area;
    std::array<double, 3> angles;
};

struct ThreadData {
    const std::vector<Point3D>* points;
    std::vector<TriangleResult>* shared_results;
    // INFO: atomic - гарантирует атомарный доступ к данным,
    // тоесть у тебя в один момент не могут читать данные из переменной и записывать новые,
    // что как раз таки и реализовано у тебя в алгоритме:
    // пока один поток не изменит значение этой переменной, все в бесконечном цикле ждут и читают её каждый раз,
    // также если не юзать атомик, у тебя вроде как, может переменная просто закешироваться и в нужный момент
    // не возьмёт изменённую переменную, а будет читать закешированное значение, поэтому в твоём случае это необходимо.
    // Ну и если в один момент времени разные потоки будут читать из неё данные, а один писать, то может возникнуть гонка данных и бб
    std::atomic<int>* turn;
    int thread_id;
    int num_threads;
};

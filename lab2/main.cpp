#include "point3d.h"
#include "math_utils.h"
#include "file_utils.h"
#include "thread_utils.h"
#include <iostream>
#include <vector>
#include <limits>

using namespace std;

int main() {
    const string filename = "points.txt";

    int N_POINTS = 0;
    while(N_POINTS <= 0) {
        cout << "Enter number of points: ";
        cin >> N_POINTS;
    }

    generate_data_file(filename, N_POINTS);

    vector<Point3D> points = read_points_from_file(filename);
    
    if(points.empty()) { cerr << "No points read\n"; return 1; }

    int choice = 0;
    while(true) {
        cout << "\nSelect mode:\n1. Single-thread\n2. Multi-thread\n3. Exit\n";
        cin >> choice;

        if (choice == 1) {
            run_single_threaded(points);
        } else if (choice == 2) {
            int num_threads = 0;
            while(num_threads<1||num_threads>400) {
                cout << "Enter number of threads (1-4): ";
                cin >> num_threads;
            }
            run_multi_thread(points,num_threads);
        } else if (choice == 3) {
            break;
        } else {
            cout << "Invalid choice\n";
        }
    }

    return 0;
}

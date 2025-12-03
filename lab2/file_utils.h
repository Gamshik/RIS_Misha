#pragma once
#include "point3d.h"
#include <string>
#include <vector>

using namespace std;

void generate_data_file(const string& filename, int n);
vector<Point3D> read_points_from_file(const string& filename);
void write_results_to_file(const string& filename, const vector<TriangleResult>& results);

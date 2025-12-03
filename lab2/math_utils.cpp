#include "math_utils.h"
#include <cmath>

Vector3D operator-(const Point3D& a, const Point3D& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

double dot_product(const Vector3D& v1, const Vector3D& v2) {
    return v1.x*v2.x + v1.y*v2.y + v1.z*v2.z;
}

Vector3D cross_product(const Vector3D& v1, const Vector3D& v2) {
    return {
        v1.y*v2.z - v1.z*v2.y,
        v1.z*v2.x - v1.x*v2.z,
        v1.x*v2.y - v1.y*v2.x
    };
}

double magnitude(const Vector3D& v) {
    return sqrt(dot_product(v, v));
}

double clamp(double v, double lo, double hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

double calculate_angle(const Point3D& p_prev, const Point3D& p_curr, const Point3D& p_next) {
    Vector3D v1 = p_prev - p_curr;
    Vector3D v2 = p_next - p_curr;
    double dot = dot_product(v1, v2);
    double mag1 = magnitude(v1);
    double mag2 = magnitude(v2);
    if (mag1 < EPSILON || mag2 < EPSILON) return 0.0;
    double cosv = dot /     (mag1 * mag2);
    cosv = clamp(cosv, -1.0, 1.0);
    return acos(cosv) * 180.0 / PI;
}

bool is_degenerate_triangle(const std::array<Point3D,3>& tri) {
    Vector3D a = tri[1] - tri[0];
    Vector3D b = tri[2] - tri[0];
    Vector3D cp = cross_product(a, b);
    return magnitude(cp) < EPSILON;
}

void process_triangle(const std::array<Point3D, 3>& tri, std::vector<TriangleResult>& local_results) {
    // INFO: ну тут чисто математика, я вообще ему это не показывал, но если чо, вот объяснение иишки

    // AI: Проверка на вырожденность
    // Вырожденный треугольник — это когда:
    // все 3 точки совпадают
    // или 2 точки совпадают
    // или они лежат на одной прямой
    // Если треугольник такой — не обрабатываем.
    if (is_degenerate_triangle(tri)) return;

    // v1 = сторона от вершины 0 → 1
    // v2 = сторона от вершины 0 → 2
    // Для площади и других вычислений удобно использовать две стороны, исходящие из одной точки.
    Vector3D v1 = tri[1] - tri[0];
    Vector3D v2 = tri[2] - tri[0];

    // Векторное произведениe - |v1 × v2| = площадь параллелограмма
    // 0.5 * |v1 × v2| - площадь треугольника
    double area = 0.5 * magnitude(cross_product(v1, v2));

    // Если площадь почти нулевая → точки почти на одной прямой:
    if (area < EPSILON) return;

    TriangleResult res;
    res.vertices = tri;
    res.area = area;

    res.angles[0] = calculate_angle(tri[1], tri[0], tri[2]);
    res.angles[1] = calculate_angle(tri[0], tri[1], tri[2]);
    res.angles[2] = calculate_angle(tri[0], tri[2], tri[1]);

    // То есть ВСЕ углы строго меньше 90°.
    // 1e-9 используется как защита от погрешностей вычислений (числа с плавающей точкой неточные).
    // Остроугольный треугольник — это треугольник, в котором все три угла являются острыми, то есть их величина меньше 90 градусов.
    if (res.angles[0] < 90.0 - 1e-9 && res.angles[1] < 90.0 - 1e-9 && res.angles[2] < 90.0 - 1e-9) {
        local_results.push_back(res);
    }
}

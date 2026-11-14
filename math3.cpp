
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <utility>

struct VEC2 {
    float x, y;
};

struct VEC3 {
    float x, y, z;
};

struct Triangle {
    int a, b, c;
    uint32_t color;
};

struct Mesh {
    std::vector<Triangle> tirangle;
    std::vector<VEC3> sommet;
};

inline float edge(VEC2 a, VEC2 b, VEC2 p) {
    return (b.x - a.x) * (p.y - a.y)
         - (b.y - a.y) * (p.x - a.x);
}

VEC3 rotate_x(VEC3 v, float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);

    VEC3 r;

    r.x = v.x;
    r.y = v.y * c - v.z * s;
    r.z = v.y * s + v.z * c;

    return r;
}

VEC3 rotate_y(VEC3 v, float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);

    VEC3 r;

    r.x = v.x * c + v.z * s;
    r.y = v.y;
    r.z = -v.x * s + v.z * c;

    return r;
}
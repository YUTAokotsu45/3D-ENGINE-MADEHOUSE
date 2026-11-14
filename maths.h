#pragma once

#include <vector>
#include <cmath>
#include <cstdint>

struct VEC2 {
    float x, y;
};

struct VEC3 {
    float x, y, z;
};

struct Triangle {
    int a, b, c;
    uint32_t color;
    int ta = -1, tb = -1, tc = -1;
};

struct Mip {                              // la « fausse » Texture : juste une image
    int w, h;
    std::vector<uint32_t> buffer_texture;
};

struct Texture {
    int w, h;
    std::vector<uint32_t> buffer_texture;
    std::vector<Mip> mips;                // plus de récursion
};

struct UV {
    VEC3 position;
    VEC2 uv;
};

struct Mesh {
    std::vector<Triangle> tirangle;
    std::vector<VEC3> sommet;
    std::vector<VEC2> uv;
    std::vector<VEC3> normal ;
};

inline float edge(VEC2 a, VEC2 b, VEC2 p) {
    return (b.x - a.x) * (p.y - a.y)
         - (b.y - a.y) * (p.x - a.x);
}

inline VEC3 rotate_x(VEC3 v, float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);

    VEC3 r;

    r.x = v.x;
    r.y = v.y * c - v.z * s;
    r.z = v.y * s + v.z * c;

    return r;
}

inline VEC3 rotate_y(VEC3 v, float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);

    VEC3 r;

    r.x = v.x * c + v.z * s;
    r.y = v.y;
    r.z = -v.x * s + v.z * c;

    return r;
}

inline VEC3 projection(
    VEC3 point, float focal,
    int width, int height,
    float distance, VEC2 origin
) {
    float depth = point.z + distance;
    if (depth <= 0.01f) depth = 0.01f;   // provisoire, en attendant le clipping

    float factor = focal / depth;

    VEC3 result;

    result.x = (width * 0.5f) + ((point.x + origin.x) * factor);
    result.y = (height * 0.5f) - ((point.y + origin.y)* factor);
    result.z = 1.f / depth;   // profondeur inversée, pour le z-buffer

    return result;
}

inline float Lerp (float a, float b, float t) {
    return a + t * (b - a) ;
}

inline VEC3 Cross (VEC3 ac, VEC3 ab ) {
    return{
        (ac.y * ab.z) - (ac.z * ab.y),
        (ac.z * ab.x) - (ac.x * ab.z),
        (ac.x * ab.y) - (ac.y * ab.x)
    } ;
}

inline VEC3 Normalize(VEC3 no) {
    float longueur = std::sqrt(no.x * no.x + no.y * no.y + no.z * no.z); // la racine, comme dans NormalClean
    if (longueur == 0.f) return {0, 0, 0} ;   // flèche nulle : on ne divise pas par 0
    return {no.x /= longueur, no.y /= longueur, no.z /= longueur};          // v.x, v.y, v.z divisés par longueur
}

inline float Dot (VEC3 nrm , VEC3 light_obj) {
    return nrm.x * light_obj.x + nrm.y * light_obj.y + nrm.z * light_obj.z ;
}
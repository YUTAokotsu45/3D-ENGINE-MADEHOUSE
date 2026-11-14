#include "model.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <vector>
#include <string>
#include <fstream>
#include <cfloat>

#define STB_IMAGE_IMPLEMENTATION   // UNE seule fois dans tout le projet
#include "stb_image.h"

std::vector<std::string> split(const std::string& s) {
    std::istringstream iss(s);
    std::vector<std::string> parts;
    std::string word;
    while (iss >> word) parts.push_back(word);
    return parts;
}

// Pas de valeur par défaut ici : elle est déjà dans la déclaration (model.h)
Mesh ObjLOAD(const std::string& file_name, uint32_t color) {
    Mesh mesh;
    std::ifstream file(file_name);
    if (!file) return mesh;   // fichier introuvable → mesh vide

    // ── 1re passe : compter, pour réserver la mémoire en un seul bloc ──
    std::string line;
    size_t v_count = 0, f_count = 0;
    while (std::getline(file, line)) {
        if (line.rfind("v ", 0) == 0) v_count++;
        else if (line.rfind("f ", 0) == 0) f_count++;
    }
    mesh.sommet.reserve(v_count);
    mesh.tirangle.reserve(f_count * 2);   // estimation : beaucoup de faces sont des carrés (2 triangles)

    // ── 2e passe : lecture réelle ──
    file.clear();                  // efface l'état « fin de fichier » de la 1re passe
    file.seekg(0, std::ios::beg);  // revient au début

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream ss(line);
        std::string prefix;
        ss >> prefix;              // lit le premier mot

        if (prefix == "v") {
            VEC3 v;
            ss >> v.x >> v.y >> v.z;
            v.z = -v.z ; 
            mesh.sommet.push_back(v);
        }
        else if (prefix == "vt") {
            // Coordonnée de texture : u v (un éventuel 3e nombre est ignoré)
            VEC2 t;
            ss >> t.x >> t.y;
            mesh.uv.push_back(t);
        }
        else if (prefix == "f") {
            // Lit TOUS les sommets de la face, quel que soit leur nombre
            std::vector<int> idx;
            std::vector<int> tidx;   // indices UV (-1 = pas d'UV)
            std::string token;
            while (ss >> token) {
                // "3", "3/5", "3/5/2" ou "3//2"
                size_t slash1 = token.find('/');

                // Sommet : le nombre avant le premier '/'
                int i = std::stoi(token.substr(0, slash1));

                // Indice négatif = compté depuis la fin des sommets déjà lus
                if (i < 0) idx.push_back((int)mesh.sommet.size() + i);
                else       idx.push_back(i - 1);   // indices .obj : 1-based → 0-based

                // UV : le nombre ENTRE le premier et le deuxième '/'
                int ti = -1;
                if (slash1 != std::string::npos) {
                    size_t slash2 = token.find('/', slash1 + 1);   // cherche APRÈS le premier
                    std::string mid = (slash2 == std::string::npos)
                        ? token.substr(slash1 + 1)
                        : token.substr(slash1 + 1, slash2 - slash1 - 1);

                    if (!mid.empty()) {   // vide pour "3//2" : pas d'UV
                        int t = std::stoi(mid);
                        ti = (t < 0) ? (int)mesh.uv.size() + t : t - 1;
                    }
                }
                tidx.push_back(ti);
            }

            // Découpage en éventail : (0, 1, 2), (0, 2, 3), (0, 3, 4)...
            for (size_t k = 1; k + 1 < idx.size(); k++)
                mesh.tirangle.push_back({idx[0], idx[k], idx[k + 1], color,
                                         tidx[0], tidx[k], tidx[k + 1]});
        }
        // Les autres lignes (vn, o, s, usemtl...) sont ignorées
    }

    // ── Sécurité : retire les triangles qui pointent hors du tableau ──
    const int n = (int)mesh.sommet.size();
    std::erase_if(mesh.tirangle, [n](const Triangle& t) {
        return t.a < 0 || t.b < 0 || t.c < 0 || t.a >= n || t.b >= n || t.c >= n;
    });

    return mesh;
}

Texture LoadTexture(const char* filename) {
    Texture texture;
    texture.w = 0;
    texture.h = 0;

    // w, h, c en LOCAL : c'est stbi_load qui les découvre et les remplit
    int w, h, c;
    unsigned char* img = stbi_load(filename, &w, &h, &c, 4);

    // Fichier introuvable ou illisible → texture vide (w = h = 0)
    if (img == nullptr) return texture;

    texture.w = w;
    texture.h = h;

    int total_size = w * h;
    std::vector<uint32_t> buffer_raw(total_size);   // déjà total_size cases → on écrit par index

    for (int i = 0; i < total_size; i++) {
        // 4 octets par pixel (R, G, B, A) → le pixel i commence à l'octet i * 4
        unsigned char r = img[i * 4];
        unsigned char g = img[i * 4 + 1];
        unsigned char b = img[i * 4 + 2];

        buffer_raw[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
    }
    stbi_image_free(img);   // les octets bruts ne servent plus

    // ── Niveau 0 : la taille réelle ──
    texture.mips.push_back({w, h, buffer_raw});

    // ── Niveaux suivants : chaque pixel = moyenne d'un carré 2×2 du niveau d'avant ──
    while (texture.mips.back().w > 1 || texture.mips.back().h > 1) {
        Mip n;
        {
            const Mip& prec = texture.mips.back();   // le grand niveau (lecture seule)
            const int pw = prec.w, ph = prec.h;
            n.w = std::max(1, pw / 2);
            n.h = std::max(1, ph / 2);
            n.buffer_texture.resize(n.w * n.h);

            for (int y = 0; y < n.h; y++) {
                for (int x = 0; x < n.w; x++) {
                    // Les 4 pixels sources : (2x, 2y) et ses voisins.
                    // std::min : si une dimension vaut déjà 1, on ne sort pas de l'image.
                    const int x0 = 2 * x, x1 = std::min(2 * x + 1, pw - 1);
                    const int y0 = 2 * y, y1 = std::min(2 * y + 1, ph - 1);

                    const uint32_t p[4] = {
                        prec.buffer_texture[y0 * pw + x0],   // haut-gauche
                        prec.buffer_texture[y0 * pw + x1],   // haut-droite
                        prec.buffer_texture[y1 * pw + x0],   // bas-gauche
                        prec.buffer_texture[y1 * pw + x1]    // bas-droite
                    };

                    // Moyenne canal par canal (A, R, G, B), puis on recombine
                    uint32_t resultat = 0;
                    for (int shift : {24, 16, 8, 0}) {
                        uint32_t somme = 0;
                        for (int k = 0; k < 4; k++) somme += (p[k] >> shift) & 0xFF;
                        resultat |= ((somme + 2) / 4) << shift;   // +2 = arrondi au plus proche
                    }

                    n.buffer_texture[y * n.w + x] = resultat;   // 2D → 1D
                }
            }
        }   // "prec" n'existe plus ici : push_back peut déplacer le tableau sans danger
        texture.mips.push_back(std::move(n));
    }

    // L'originale reste aussi dans texture.buffer_texture (w, h déjà remplis)
    texture.buffer_texture = std::move(buffer_raw);
    return texture;
}

void NormaliserModele(Mesh& mesh) {
    if (mesh.sommet.empty()) return;

    // 1. Boîte englobante : coins min et max sur chaque axe
    VEC3 mn = { FLT_MAX,  FLT_MAX,  FLT_MAX};
    VEC3 mx = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
    for (const VEC3& v : mesh.sommet) {
        mn.x = std::min(mn.x, v.x);  mx.x = std::max(mx.x, v.x);
        mn.y = std::min(mn.y, v.y);  mx.y = std::max(mx.y, v.y);
        mn.z = std::min(mn.z, v.z);  mx.z = std::max(mx.z, v.z);
    }

    // 2. Centre de la boîte et plus grande dimension
    VEC3 center = {(mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, (mn.z + mx.z) * 0.5f};
    float size = std::max({mx.x - mn.x, mx.y - mn.y, mx.z - mn.z});
    if (size == 0.f) return;
    float scale = 2.f / size;   // la plus grande dimension devient 2 (de -1 à 1)

    // 3. Recentrage + mise à l'échelle de chaque sommet
    for (VEC3& v : mesh.sommet) {
        v.x = (v.x - center.x) * scale;
        v.y = (v.y - center.y) * scale;
        v.z = (v.z - center.z) * scale;
    }
}
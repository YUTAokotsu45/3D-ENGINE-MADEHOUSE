#include "rendering.h"
#include "lumiere.h"
#include "maths.h"
#include <cmath>
#include <algorithm>
#include <vector>

// Définition UNIQUE des buffers (déclarés extern dans rendering.h)
uint32_t frame_buffer[W * H];
float    z_buffer[W * H];

struct Camera {
    VEC3 position ; 
    Mesh& vision ;
} ;

void TriangleFilled(
    UV a, UV b, UV c, uint32_t color,
    float light_a, float light_b,
    float light_c, const Mip& texture, 
    float opacity, uint32_t fog_color,
    float spec_a, float spec_b, float spec_c,
    int min_y, int max_y
) {

    if (texture.w <= 0 || texture.h <= 0) return;
    if (texture.buffer_texture.size() < (size_t)texture.w * texture.h) return;

    VEC2 a2 = {a.position.x, a.position.y};
    VEC2 b2 = {b.position.x, b.position.y};
    VEC2 c2 = {c.position.x, c.position.y};

    int minX = std::max(0, (int)std::floor(std::min({a.position.x, b.position.x, c.position.x})));
    int maxX = std::min(W - 1, (int)std::ceil(std::max({a.position.x, b.position.x, c.position.x})));

    int minY = std::max(0, (int)std::floor(std::min({a.position.y, b.position.y, c.position.y})));
    int maxY = std::min(H - 1, (int)std::ceil(std::max({a.position.y, b.position.y, c.position.y})));

    minY = std::max(minY, min_y);   // on ne commence pas avant le haut de la bande
    maxY = std::min(maxY, max_y);   // on ne dépasse pas le bas de la bande

    if (minX > maxX || minY > maxY) return;   // triangle hors écran ou hors de la bande

    float area = edge(a2, b2, c2);
    if (area == 0.f) return;

    float inv_area = 1.f / area;

    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {

            VEC2 p{x + 0.5f, y + 0.5f};

            float w0 = edge(b2, c2, p) * inv_area;
            float w1 = edge(c2, a2, p) * inv_area;
            float w2 = edge(a2, b2, p) * inv_area;

            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {

                float inv_z = w0 * a.position.z + w1 * b.position.z + w2 * c.position.z;

                int i = y * W + x;

                if (inv_z > z_buffer[i]) {

                    float u = (
                        w0 * a.uv.x * a.position.z +
                        w1 * b.uv.x * b.position.z +
                        w2 * c.uv.x * c.position.z
                    ) / inv_z;

                    float v = (
                        w0 * a.uv.y * a.position.z +
                        w1 * b.uv.y * b.position.z +
                        w2 * c.uv.y * c.position.z
                    ) / inv_z;

                    float tx = u * (texture.w - 1);
                    float ty = (1.f - v) * (texture.h - 1); 

                    if (tx < 0) tx = 0;
                    if (tx >= texture.w) tx = texture.w - 1;

                    if (ty < 0) ty = 0;
                    if (ty >= texture.h) ty = texture.h - 1;
                    int x0 = (int)std::floor(tx);
                    int x1 = std::min(x0 + 1, texture.w - 1);

                    int y0 = (int)std::floor(ty);
                    int y1 = std::min(y0 + 1, texture.h - 1);

                    float dist_x = tx - x0;
                    float dist_y = ty - y0;

                    uint32_t c00 = texture.buffer_texture[y0 * texture.w + x0];
                    uint32_t c10 = texture.buffer_texture[y0 * texture.w + x1];
                    uint32_t c01 = texture.buffer_texture[y1 * texture.w + x0];
                    uint32_t c11 = texture.buffer_texture[y1 * texture.w + x1];

                    uint32_t tex_color = 0;   // 0 : l'alpha est maintenant filtré lui aussi (shift 24)

                    for (int shift : {24, 16, 8, 0}) {
                        float p00 = (c00 >> shift) & 0xFF;
                        float p10 = (c10 >> shift) & 0xFF;
                        float p01 = (c01 >> shift) & 0xFF;
                        float p11 = (c11 >> shift) & 0xFF;

                        float top = Lerp(p00, p10, dist_x);
                        float bottom = Lerp(p01, p11, dist_x);
                        float result = Lerp(top, bottom, dist_y);

                        tex_color |= (uint32_t)result << shift;
                    }

                    // Alpha calculé APRÈS le filtrage : sinon il ne voit pas la vraie couleur
                    float alpha_tex = static_cast<float>((tex_color >> 24) & 0xFF) / 255.f;
                    float alpha = opacity * alpha_tex ;

                    float light = w0 * light_a + w1 * light_b + w2 * light_c;

                    uint32_t red = std::min(255u, (uint32_t)(((tex_color >> 16) & 0xFF) * light));   // plafond : le reflet peut dépasser 1
                    uint32_t green = std::min(255u, (uint32_t)(((tex_color >> 8) & 0xFF) * light));
                    uint32_t blue = std::min(255u, (uint32_t)((tex_color & 0xFF) * light));

                    uint32_t fond = frame_buffer[i];
                    float fog_r = (fog_color >> 16) & 0xFF;
                    float fog_g = (fog_color >> 8)  & 0xFF;
                    float fog_b =  fog_color        & 0xFF;

                    float fond_r = (fond >> 16) & 0xFF;
                    float fond_g = (fond >> 8) & 0xFF;
                    float fond_b = (fond >> 0) & 0xFF;

                    float depth_z = 1.f / inv_z;
                    float begin = 7.f;
                    float end = 15.f ;

                    // ── Étape 1 : BROUILLARD, seulement si le pixel est plus loin que begin ──
                    if (depth_z > begin) {
                        float f = std::clamp((depth_z - begin) / (end - begin), 0.f, 1.f);
                        red   = static_cast<uint32_t>(Lerp(red,   fog_r, f));   // 1
                        green = static_cast<uint32_t>(Lerp(green, fog_g, f));   // 2
                        blue  = static_cast<uint32_t>(Lerp(blue,  fog_b, f));   // 3
                    }

                    // ── Étape 2 : TRANSPARENCE, toujours (alpha = 1 → aucun changement) ──
                    red   = static_cast<uint32_t>(Lerp(fond_r, red,   alpha));
                    green = static_cast<uint32_t>(Lerp(fond_g, green, alpha));
                    blue  = static_cast<uint32_t>(Lerp(fond_b, blue,  alpha));

                    // le reflet, interpolé comme la lumière
                    float spec = w0 * spec_a + w1 * spec_b + w2 * spec_c;
                    uint32_t s = (uint32_t)(spec * 255.f);

                    // ajouté en BLANC : la même quantité sur les 3 canaux
                    red   = std::min(255u, red   + s);
                    green = std::min(255u, green + s);
                    blue  = std::min(255u, blue  + s);
                    // ── Étape 3 : UNE seule écriture à l'écran ─
                    frame_buffer[i] =
                        0xFF000000 |
                        (red << 16) |
                        (green << 8) |
                        blue;

                    // ── Étape 4 : seuls les pixels opaques écrivent leur profondeur ──
                    // (un pixel transparent ne doit pas cacher ce qui est derrière lui)
                    if (alpha >= 1.f) z_buffer[i] = inv_z ;
                }
            }
        }
    }
}

void RenderMesh(
    const Mesh& mesh,
    float angle_x,
    float angle_y,
    float distance,
    VEC2 origin,
    const Texture& texture,
    VEC3 cam_pos,
    float opacity,
    uint32_t fog_color,
    int nb_light
) {
    // Pas de mipmaps = texture pas chargée : rien à dessiner
    if (texture.mips.empty()) return;

    // Jamais plus de lumières que le tableau n'en contient
    nb_light = std::clamp(nb_light, 0, NB_LIGHT_MAX);

    // Chaque lumière : normalisée, puis tournée à l'envers (l'astuce de light_obj).
    // Taille FIXE (NB_LIGHT_MAX) : un tableau C++ standard ne peut pas avoir une
    // taille qui dépend d'une variable comme nb_light.
    VEC3 lumieres_obj[NB_LIGHT_MAX];
    for (int k = 0; k < nb_light; k++) {
        VEC3 l = Normalize(directional_light[k]);
        lumieres_obj[k] = rotate_x(rotate_y(l, -angle_y), -angle_x);
    }

    // Lumière principale dans le repère du MONDE (pour NormalRender, étape 2)
    VEC3 light = Normalize(directional_light[0]);

    const int n  = (int)mesh.sommet.size();
    const int nt = (int)mesh.tirangle.size();

    std::vector<VEC3> rotated(n);
    std::vector<VEC3> projected(n);
    std::vector<uint32_t> colors(nt);
    std::vector<float> lights(nt);

    // La caméra vue depuis l'objet : on retire la position de l'objet, puis on tourne à l'envers
    VEC3 cam_obj = rotate_x(rotate_y({cam_pos.x - origin.x,
                                      cam_pos.y - origin.y,
                                      cam_pos.z - distance}, -angle_y), -angle_x);

    // Lumière de chaque sommet (Gouraud). Chaque sommet écrit dans SA case → OpenMP sûr
    std::vector<float> vertex_light(n);
    std::vector<float> vertex_spec(n);
    std::vector<VEC3> vision(n) ; 
    #pragma omp parallel for
    for (int index = 0; index < n; index++) {
        VEC3 nrm = mesh.normal[index];

        // L'ambiante, UNE seule fois
        vertex_light[index] = 0.25f;
        vertex_spec[index]  = 0.f; 
        // Puis chaque lumière ajoute sa diffuse (Lambert) et son reflet (Blinn-Phong)
        for (int k = 0; k < nb_light; k++) {
            vertex_light[index] += 0.75f * std::max(0.f, Dot(nrm, lumieres_obj[k]));
            vertex_spec[index] += BuiRender(mesh.sommet[index], cam_obj, lumieres_obj[k], nrm);
        }
    }

    #pragma omp parallel for
    for (int index = 0; index < n; index++) {

        //c est ici ou on verifie 
        rotated[index] = rotate_y(
            rotate_x(mesh.sommet[index], angle_x),
            angle_y
        );
        vision[index] = {
            rotated[index].x + origin.x,
            rotated[index].y + origin.y,
            rotated[index].z + distance  
        }; 
        projected[index] = projection(rotated[index], 500.f, W, H, distance, origin);
    }

    // ── Préparation de chaque triangle (culling, couleur) : UNE fois, en parallèle ──
    // Chaque triangle écrit dans SA case de colors[] → pas de conflit entre cœurs
    #pragma omp parallel for
    for (int tri_dex = 0; tri_dex < nt; tri_dex++) {
        const Triangle& t = mesh.tirangle[tri_dex];
        const float near = 0.1f;
        if (vision[t.a].z < near || vision[t.b].z < near || vision[t.c].z < near) {
            colors[tri_dex] = 0;
            continue;
        }
        VEC3 point_a = projected[t.a];
        VEC3 point_b = projected[t.b];
        VEC3 point_c = projected[t.c];

        float area = edge(
            {point_a.x, point_a.y},
            {point_b.x, point_b.y},
            {point_c.x, point_c.y}
        );

        if (area >= 0.f) {
            colors[tri_dex] = 0;
            continue;
        }

        float lambert = NormalRender(
            rotated[t.a],
            rotated[t.b],
            rotated[t.c],
            light
        );

        lights[tri_dex] = lambert;

        uint32_t red   = (uint32_t)(((t.color >> 16) & 0xFF) * lambert);
        uint32_t green = (uint32_t)(((t.color >> 8) & 0xFF) * lambert);
        uint32_t blue  = (uint32_t)((t.color & 0xFF) * lambert);

        colors[tri_dex] =
            0xFF000000 |
            (red << 16) |
            (green << 8) |
            blue;
    }

    // ── Dessin en BANDES : chaque cœur dessine sa tranche horizontale de l'écran ──
    // Deux bandes ne touchent jamais les mêmes lignes → aucun pixel écrit par deux cœurs
    const int nb_bande = 1;
    const int nb_B = H / nb_bande;   // 600 / 10 = 60 lignes par bande

    #pragma omp parallel for schedule(dynamic)
    for (int bande = 0; bande < nb_bande; bande++) {
        int y_min = bande * nb_B;
        int y_max = (bande == nb_bande - 1) ? H - 1 : y_min + nb_B - 1;   // la dernière va jusqu'en bas

        // Dessin séquentiel
        for (int tri_dex = 0; tri_dex < nt; tri_dex++) {

            if (colors[tri_dex] == 0) continue;

            const Triangle& t = mesh.tirangle[tri_dex];

            if (t.ta < 0 || t.tb < 0 || t.tc < 0) continue;

            if (t.ta >= (int)mesh.uv.size() ||
                t.tb >= (int)mesh.uv.size() ||
                t.tc >= (int)mesh.uv.size()) continue;

            UV a = {projected[t.a], mesh.uv[t.ta]};
            UV b = {projected[t.b], mesh.uv[t.tb]};
            UV c = {projected[t.c], mesh.uv[t.tc]};

            // ── Mipmap : choix du niveau selon la distance du triangle ──
            // Distance ×2 → l'objet paraît 2× plus petit → niveau + 1 (texture 2× plus petite)
            const float d_base = 2.f;   // distance où la pleine taille est parfaite (à régler à l'œil)
            const float z_moyen = (vision[t.a].z + vision[t.b].z + vision[t.c].z) / 3.f;

            int niveau = (int)std::log2(z_moyen / d_base);
            niveau = std::clamp(niveau, 0, (int)texture.mips.size() - 1);   // ni < 0, ni trop loin

            const Mip& tex_niveau = texture.mips[niveau];   // on pointe, aucune copie

            TriangleFilled(a, b, c, colors[tri_dex],
                        vertex_light[t.a], vertex_light[t.b], vertex_light[t.c],
                        tex_niveau, opacity, fog_color, vertex_spec[t.a],  vertex_spec[t.b],  vertex_spec[t.c],
                        y_min, y_max);   // la bande
        }
    }
}

bool TriangleIN(VEC2 A, VEC2 B, VEC2 C, VEC2 P) {
    float total = edge(A, B, C);

    if (total == 0) return false;

    float alpha = edge(P, B, C) / total;
    float beta  = edge(P, C, A) / total;
    float gamma = edge(P, A, B) / total;

    return alpha >= 0 && beta >= 0 && gamma >= 0;
}

inline VEC2 PixelProjection (int i) {
    return {
        static_cast<float>(i % W),   // x : la position dans la ligne
        static_cast<float>(i / W)    // y : le numéro de la ligne
    };
}
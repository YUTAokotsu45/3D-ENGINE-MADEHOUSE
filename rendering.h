#pragma once

#include <cstdint>
#include "maths.h"
#include "lumiere.h"
#include "model.h"

constexpr int W = 800;
constexpr int H = 600;

extern uint32_t frame_buffer[W * H];
extern float    z_buffer[W * H];

void TriangleFilled(
    UV a, UV b, UV c, uint32_t color,
    float light_a, float light_b, float light_c,
    const Mip& texture, float opacity = 1,   // UN niveau de mipmap (choisi par RenderMesh)
    uint32_t fog_color = 0xFF888888,    // couleur du brouillard (= le gris du fond par défaut)
    float spec_a = 0.f, float spec_b = 0.f, float spec_c = 0.f,     // reflet de chaque sommet (ajouté en blanc)
    int min_y = 0, int max_y = H - 1);  // bande de lignes à dessiner (par défaut : tout l'écran)

void RenderMesh(
    const Mesh& mesh,
    float angle_x,
    float angle_y,
    float distance,
    VEC2 origin,
    const Texture& texture,
    VEC3 cam_pos,
    float opacity = 1,
    uint32_t fog_color = 0xFF888888,
    int nb_light = NB_LIGHT_MAX   // combien de lumières utiliser (1 ou 2)
);

// Un objet de la scène : CE QUE c'est (mesh + texture, partagés) et OÙ il est
struct Objet {
    // Pointeurs : plusieurs objets peuvent partager le MÊME mesh / la MÊME texture
    // (chargés une seule fois). const : un objet lit son modèle, il ne le modifie pas.
    const Mesh*    mesh    = nullptr;
    const Texture* texture = nullptr;

    VEC3  position = {0.f, 0.f, 0.f};   // décalage dans la scène (x, y, profondeur)
    float angle_x  = 0.f;               // rotation propre de l'objet
    float angle_y  = 0.f;
    float opacity  = 1.f;               // 1 = opaque
};

bool TriangleIN(VEC2 A, VEC2 B, VEC2 C, VEC2 P);
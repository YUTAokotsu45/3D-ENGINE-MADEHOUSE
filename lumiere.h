#pragma once

#include "maths.h"

// Nombre maximum de lumières directionnelles
constexpr int NB_LIGHT_MAX = 2;

// Directions VERS chaque lumière : z négatif = du côté de la caméra
extern VEC3 directional_light[NB_LIGHT_MAX];

// Lambert à partir des sommets DÉJÀ tournés (light doit être normalisée)
float NormalRender(VEC3 side1, VEC3 side2, VEC3 side3, VEC3 light);

// Lumière par sommet (Gouraud)
float GourRender(const Mesh& mesh, int sommet_index, VEC3 light);

void NormalClean(Mesh& mesh);

float BuiRender (VEC3 p, VEC3 cam_obj, VEC3 light_obj, VEC3 nrm);
#include "lumiere.h"
#include <cmath>
#include <algorithm>
#include <vector>

// Direction VERS la lumière : z négatif = du côté de la caméra
// (avec z = +1, la lumière venait de derrière l'objet)
VEC3 directional_light[NB_LIGHT_MAX] = {
    {0.f, 1.f, -1.f},     // lumière 0 : la principale, d'en haut, côté caméra
    {-1.f, 0.f, -0.5f}    // lumière 1 : par la gauche
};

float NormalRender(VEC3 side1, VEC3 side2, VEC3 side3, VEC3 light) {

    VEC3 ab = {
        side2.x - side1.x,
        side2.y - side1.y,
        side2.z - side1.z
    };

    VEC3 ac = {
        side3.x - side1.x,
        side3.y - side1.y,
        side3.z - side1.z
    };

    // Normale vers l'extérieur : cross(ac, ab)
    VEC3 normal = {
        (ac.y * ab.z) - (ac.z * ab.y),
        (ac.z * ab.x) - (ac.x * ab.z),
        (ac.x * ab.y) - (ac.y * ab.x)
    };

    float large = std::sqrt(
        normal.x * normal.x +
        normal.y * normal.y +
        normal.z * normal.z
    );

    const float ambient = 0.25f;

    // Triangle dégénéré : ambiante (avant : 0 → triangle noir)
    if (large == 0.f) return ambient;

    float lambert =
        (normal.x * light.x +
         normal.y * light.y +
         normal.z * light.z) / large;

    // Bloqué à 0 pour les faces dos à la lumière → résultat toujours dans [0.25, 1]
    return ambient + (1.f - ambient) * std::max(0.f, lambert);
}

void NormalClean(Mesh& mesh) {
    mesh.normal.assign(mesh.sommet.size(), {0.f, 0.f, 0.f});
    // Pour chaque triangle du modèle...
    for (const Triangle& t : mesh.tirangle) {

        // ...ses 3 coins : t.a, t.b, t.c sont les NUMÉROS des sommets
        VEC3 side1 = mesh.sommet[t.a];
        VEC3 side2 = mesh.sommet[t.b];
        VEC3 side3 = mesh.sommet[t.c];

        VEC3 ab = {
            side2.x - side1.x,
            side2.y - side1.y,
            side2.z - side1.z
        };

        VEC3 ac = {
            side3.x - side1.x,
            side3.y - side1.y,
            side3.z - side1.z
        };

        // Normale vers l'extérieur : cross(ac, ab)
        VEC3 n = {
            (ac.y * ab.z) - (ac.z * ab.y),
            (ac.z * ab.x) - (ac.x * ab.z),
            (ac.x * ab.y) - (ac.y * ab.x)
        };

        // TODO 1 : ajouter "normal" à mesh.normal[t.a], mesh.normal[t.b], mesh.normal[t.c]
        mesh.normal[t.a].x += n.x;  mesh.normal[t.a].y += n.y;  mesh.normal[t.a].z += n.z;
        mesh.normal[t.b].x += n.x;  mesh.normal[t.b].y += n.y;  mesh.normal[t.b].z += n.z;
        mesh.normal[t.c].x += n.x;  mesh.normal[t.c].y += n.y;  mesh.normal[t.c].z += n.z; 
    }

    // TODO 2 : pour chaque normale de mesh.normal, calculer sa longueur (ton "large")
    //          et diviser x, y, z par cette longueur
    for (VEC3& no : mesh.normal ) {
        float longuer = std::sqrt(no.x * no.x + no.y * no.y + no.z * no.z) ;
        if (longuer > 0) {
            no.x /= longuer ; 
            no.y /= longuer ; 
            no.z /= longuer ;
        }
    }
}
float GourRender(const Mesh& mesh, int sommet_index, VEC3 light) {
    std::vector<float> normales_edge;
    for (int index = 0 ; index < mesh.tirangle.size() ; index++) {
        const Triangle& t = mesh.tirangle[index] ; 

        if (t.a == sommet_index || t.b == sommet_index || t.c == sommet_index) {
            VEC3 a = mesh.sommet[t.a];
            VEC3 b = mesh.sommet[t.b];
            VEC3 c = mesh.sommet[t.c];
            normales_edge.push_back(NormalRender(a, b, c, light)) ; 
        }
    }
    float normal = 0.f;
    for (int jitex = 0 ; jitex < normales_edge.size() ; jitex ++) normal += normales_edge[jitex] ;
    if (normales_edge.size() > 0) normal /= normales_edge.size() ;
    return normal ;
}

float BuiRender (VEC3 p, VEC3 cam_obj, VEC3 light_obj, VEC3 nrm) {
    VEC3 V = Normalize({cam_obj.x - p.x, cam_obj.y - p.y, cam_obj.z - p.z}) ;
    VEC3 Hv = Normalize({light_obj.x + V.x, light_obj.y + V.y, light_obj.z + V.z}) ;
    return std::pow(std::max(0.f, Dot(nrm, Hv)), 32.f);  
}
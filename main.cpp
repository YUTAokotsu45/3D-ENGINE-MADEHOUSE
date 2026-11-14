#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <iostream>
#include <algorithm>
#include <vector>

#include "rendering.h"   // inclut déjà maths.h, lumiere.h et model.h (+ la struct Objet)

// Couleur du fond ET du brouillard (ex. 0xFF888888 = gris, 0xFF87CEEB = bleu ciel)
const uint32_t couleur_fond = 0xFF87CEEB;

int main(int argc, char* argv[]) {

    // ── Fenêtre SDL ──
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "Erreur SDL_Init : " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window*   window = SDL_CreateWindow("Window Engine 3D", W, H, SDL_WINDOW_RESIZABLE);
    SDL_Renderer* render = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    SDL_Texture*  screen = render ? SDL_CreateTexture(render, SDL_PIXELFORMAT_ARGB8888,
                                                      SDL_TEXTUREACCESS_STREAMING, W, H)
                                  : nullptr;

    // Libère tout ce qui a été créé (SDL_Destroy... accepte nullptr sans problème)
    auto quitter = [&](const char* erreur) {
        if (erreur) std::cerr << erreur << std::endl;
        SDL_DestroyTexture(screen);
        SDL_DestroyRenderer(render);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return erreur ? 1 : 0;
    };

    if (screen == nullptr) return quitter(SDL_GetError());

    SDL_SetRenderLogicalPresentation(render, W, H, SDL_LOGICAL_PRESENTATION_LETTERBOX);  // garde le 4:3
    SDL_SetRenderVSync(render, 0);   // 0 = FPS libres

    // ── Ressources : chargées UNE seule fois, partagées par les objets ──
    Mesh piece_mesh = ObjLOAD(
        R"(C:\Users\LENOVO\Documents\Moteur3D\KayKit_Prototype_Bits_1.1_FREE\Assets\obj\Coin_B.obj)");
    Texture texture = LoadTexture(
        R"(C:\Users\LENOVO\Documents\Moteur3D\KayKit_Prototype_Bits_1.1_FREE\Textures\prototypebits_texture.png)");

    if (piece_mesh.sommet.empty())        return quitter("Impossible de charger le modele OBJ !");
    if (texture.w <= 0 || texture.h <= 0) return quitter("Impossible de charger la texture !");

    // Test des mipmaps : on doit voir 11 tailles, de 1024x1024 à 1x1
    for (const Mip& m : texture.mips) std::cout << m.w << "x" << m.h << std::endl;
    if (piece_mesh.uv.empty())            return quitter("Aucune coordonnee UV dans le modele !");

    NormaliserModele(piece_mesh);   // centré sur l'origine, taille [-1, 1]
    NormalClean(piece_mesh);        // normales lissées (une seule fois)

    // ── La scène : plusieurs objets qui POINTENT vers le même mesh ──
    std::vector<Objet> scene;

    Objet piece;
    piece.mesh     = &piece_mesh;   // & = l'adresse : l'objet ne copie pas le modèle
    piece.texture  = &texture;
    piece.position = {0.f, 0.f, 0.f};
    scene.push_back(piece);

    Objet piece2 = piece;                    // même mesh, même texture...
    piece2.position = {2.5f, 0.f, 3.f};      // ...mais ailleurs (à droite, plus loin)
    piece2.angle_y  = 1.f;                   // et tournée autrement
    scene.push_back(piece2);

    Objet piece3 = piece;
    piece3.position = {-2.5f, 0.f, 6.f};     // à gauche, encore plus loin (dans le brouillard)
    scene.push_back(piece3);

    // ── La « caméra » : les touches déplacent et tournent TOUTE la scène ──
    float distance = 5.f;
    VEC2  origin   = {0.f, 0.f};
    float angle_y  = 20.f * SDL_PI_F / 180.f;   // toupie (gauche/droite)
    float angle_x  = 20.f * SDL_PI_F / 180.f;   // bascule (haut/bas)
    const float vitesse = 5.f;

    // ── Compteur de FPS (moyenne lissée) ──
    float fps_smooth  = 60.f;
    float title_timer = 0.f;

    bool running = true;
    Uint64 last_ticks = SDL_GetTicksNS();

    while (running) {

        // Temps écoulé depuis l'image précédente (en secondes)
        Uint64 now = SDL_GetTicksNS();
        float delta = (now - last_ticks) / 1e9f;
        last_ticks = now;

        if (delta > 0.f) fps_smooth = fps_smooth * 0.95f + (1.f / delta) * 0.05f;

        title_timer += delta;
        if (title_timer >= 0.25f) {
            title_timer = 0.f;
            char title[64];
            SDL_snprintf(title, sizeof title, "Window Engine 3D - %.0f FPS (%.2f ms)",
                         fps_smooth, 1000.f / fps_smooth);
            SDL_SetWindowTitle(window, title);
        }

        // ── Événements et clavier ──
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
        }

        // Scancodes = position PHYSIQUE (QWERTY). AZERTY : Z = W, Q = A, A = Q.
        const bool* key = SDL_GetKeyboardState(nullptr);
        const float pas = vitesse * delta;

        if (key[SDL_SCANCODE_ESCAPE]) running = false;

        if (key[SDL_SCANCODE_Q]) distance -= pas;   // A : rapproche
        if (key[SDL_SCANCODE_E]) distance += pas;   // E : éloigne

        if (key[SDL_SCANCODE_W]) origin.y += pas;   // Z : haut
        if (key[SDL_SCANCODE_S]) origin.y -= pas;   // S : bas
        if (key[SDL_SCANCODE_A]) origin.x -= pas;   // Q : gauche
        if (key[SDL_SCANCODE_D]) origin.x += pas;   // D : droite

        if (key[SDL_SCANCODE_UP])    angle_x -= pas;
        if (key[SDL_SCANCODE_DOWN])  angle_x += pas;
        if (key[SDL_SCANCODE_LEFT])  angle_y -= pas;
        if (key[SDL_SCANCODE_RIGHT]) angle_y += pas;

        // ── 1. Effacer ──
        std::fill(frame_buffer, frame_buffer + W * H, couleur_fond);
        std::fill(z_buffer, z_buffer + W * H, 0.f);

        // ── 2. Dessiner CHAQUE objet de la scène ──
        // Position finale = position de l'objet + déplacement global (tes touches)
        for (const Objet& o : scene) {
            RenderMesh(*o.mesh,                            // * : on suit le pointeur jusqu'au mesh
                       angle_x + o.angle_x,
                       angle_y + o.angle_y,
                       distance + o.position.z,             // profondeur
                       {origin.x + o.position.x,            // décalage gauche/droite
                        origin.y + o.position.y},           // décalage haut/bas
                       *o.texture,
                       {0.f, 0.f, 0.f},                     // cam_pos : la caméra est à l'origine
                       o.opacity,
                       couleur_fond);                       // brouillard = couleur du fond
        }

        // ── 3. Afficher ──
        SDL_UpdateTexture(screen, nullptr, frame_buffer, W * sizeof(uint32_t));
        SDL_RenderTexture(render, screen, nullptr, nullptr);
        SDL_RenderPresent(render);
    }

    return quitter(nullptr);
}
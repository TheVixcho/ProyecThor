#include "LoadingSpinner.h"
#include "stb_image.h"
#include <GL/glew.h>
#include <cmath>
#include <algorithm>

namespace ProyecThor::UI {

namespace {

// Textura del icono de la app, cargada una sola vez. proyecthor.png no
// llega a la GPU en ningun otro lugar hoy: main.cpp solo lo usa para
// glfwSetWindowIcon() (pixels crudos, liberados de inmediato) — este es el
// primer/unico uso como textura real.
ImTextureID g_IconTex            = (ImTextureID)0;
bool        g_IconLoadAttempted  = false;

ImTextureID GetAppIconTexture()
{
    if (g_IconLoadAttempted) return g_IconTex;
    g_IconLoadAttempted = true;

    int w, h, n;
    unsigned char* pixels = stbi_load("proyecthor.png", &w, &h, &n, 4);
    if (!pixels) return (ImTextureID)0;

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    stbi_image_free(pixels);

    g_IconTex = (ImTextureID)(intptr_t)tex;
    return g_IconTex;
}

} // namespace

void DrawLoadingSpinner(ImDrawList* dl, ImVec2 center, float radius)
{
    ImTextureID tex = GetAppIconTexture();

    // Ciclo "Discord boot": gira libre varias vueltas -> desacelera hasta
    // quedar derecho -> pausa -> repite.
    constexpr float kSpinPhase  = 1.1f;
    constexpr float kEasePhase  = 0.5f;
    constexpr float kPausePhase = 0.6f;
    constexpr float kCycle      = kSpinPhase + kEasePhase + kPausePhase;
    constexpr float kSpinTurns  = 2.0f;

    float t = std::fmod(static_cast<float>(ImGui::GetTime()), kCycle);
    float angleDeg;

    if (t < kSpinPhase)
    {
        float local = t / kSpinPhase;
        angleDeg = local * kSpinTurns * 360.0f;
    }
    else if (t < kSpinPhase + kEasePhase)
    {
        float local = (t - kSpinPhase) / kEasePhase;
        float startAngle = kSpinTurns * 360.0f;
        float endAngle   = (kSpinTurns + 1.0f) * 360.0f; // "derecho" otra vez
        float eased = 1.0f - std::pow(1.0f - local, 3.0f); // ease-out cubico
        angleDeg = startAngle + (endAngle - startAngle) * eased;
    }
    else
    {
        angleDeg = 0.0f; // pausa, alineado
    }

    float angleRad = angleDeg * (3.14159265f / 180.0f);
    float c = std::cos(angleRad), s = std::sin(angleRad);

    if (tex)
    {
        ImVec2 corners[4] = { {-1,-1}, {1,-1}, {1,1}, {-1,1} };
        ImVec2 pts[4];
        for (int i = 0; i < 4; i++) {
            float rx = corners[i].x * radius, ry = corners[i].y * radius;
            pts[i] = { center.x + rx * c - ry * s, center.y + rx * s + ry * c };
        }
        dl->AddImageQuad(tex, pts[0], pts[1], pts[2], pts[3],
                         {0,0}, {1,0}, {1,1}, {0,1}, IM_COL32(255,255,255,255));
    }
    else
    {
        // El icono no cargo (proyecthor.png no esta en el cwd del proceso):
        // fallback simple para no dejar el indicador invisible del todo.
        dl->AddCircle(center, radius, IM_COL32(255,255,255,90), 24, 1.5f);
        ImVec2 dot = { center.x + radius * c, center.y + radius * s };
        dl->AddCircleFilled(dot, radius * 0.18f, IM_COL32(255,255,255,220), 12);
    }
}

} // namespace ProyecThor::UI

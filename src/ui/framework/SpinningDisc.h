#pragma once
#include <imgui.h>

namespace ProyecThor::UI {

// Disco de vinilo animado, reutilizable en cualquier lugar que necesite un
// indicador visual de "reproduciendo audio" (ver Monitor Preview).
struct SpinningDiscState {
    float rotationAngle = 0.0f;
    float currentSpeed  = 0.0f;
};

// Llamar una vez por frame. targetSpeed en radianes/segundo (0 si esta
// pausado) -- la velocidad actual se acerca a la target con una rampa suave
// en vez de saltar de golpe.
void UpdateSpinningDisc(SpinningDiscState& state, float targetSpeed, float dt);

// Dibuja el disco centrado en 'center' con radio 'radius'. albumArt = 0
// dibuja un centro generico (glifo de nota); si hay textura, la muestra
// recortada en circulo.
void DrawSpinningDisc(ImDrawList* dl, ImVec2 center, float radius,
                     const SpinningDiscState& state, ImTextureID albumArt);

} // namespace ProyecThor::UI

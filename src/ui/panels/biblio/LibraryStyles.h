#pragma once
#include <imgui.h>

// =============================================================================
//  Paleta de botones (colores base / hover / active / texto)
//  Scope de archivo: se usa con static constexpr en cada TU que incluya esto.
// =============================================================================

// Importar — Azul Cobalto
static constexpr ImVec4 k_BtnImport  = { 0.15f, 0.35f, 0.70f, 1.00f };
static constexpr ImVec4 k_BtnImportH = { 0.20f, 0.45f, 0.85f, 1.00f };
static constexpr ImVec4 k_BtnImportA = { 0.12f, 0.28f, 0.55f, 1.00f };
static constexpr ImVec4 k_BtnImportT = { 0.90f, 0.95f, 1.00f, 1.00f };

// Actualizar — Verde Bosque Azulado
static constexpr ImVec4 k_BtnRefresh  = { 0.10f, 0.45f, 0.30f, 1.00f };
static constexpr ImVec4 k_BtnRefreshH = { 0.15f, 0.60f, 0.40f, 1.00f };
static constexpr ImVec4 k_BtnRefreshA = { 0.08f, 0.35f, 0.22f, 1.00f };
static constexpr ImVec4 k_BtnRefreshT = { 0.80f, 1.00f, 0.90f, 1.00f };

// Eliminar — Rojo Carmesi Desaturado
static constexpr ImVec4 k_BtnDel  = { 0.60f, 0.20f, 0.25f, 1.00f };
static constexpr ImVec4 k_BtnDelH = { 0.75f, 0.25f, 0.30f, 1.00f };
static constexpr ImVec4 k_BtnDelA = { 0.45f, 0.15f, 0.18f, 1.00f };
static constexpr ImVec4 k_BtnDelT = { 1.00f, 0.90f, 0.90f, 1.00f };

// Nuevo / Guardar — Verde Esmeralda
static constexpr ImVec4 k_BtnGreen  = { 0.12f, 0.50f, 0.35f, 1.00f };
static constexpr ImVec4 k_BtnGreenH = { 0.18f, 0.65f, 0.45f, 1.00f };
static constexpr ImVec4 k_BtnGreenA = { 0.09f, 0.40f, 0.28f, 1.00f };
static constexpr ImVec4 k_BtnGreenT = { 0.85f, 1.00f, 0.95f, 1.00f };

// Cancelar / Neutro — Gris Plomo Frio
static constexpr ImVec4 k_BtnNeutral  = { 0.20f, 0.22f, 0.26f, 1.00f };
static constexpr ImVec4 k_BtnNeutralH = { 0.28f, 0.30f, 0.35f, 1.00f };
static constexpr ImVec4 k_BtnNeutralA = { 0.15f, 0.16f, 0.19f, 1.00f };
static constexpr ImVec4 k_BtnNeutralT = { 0.80f, 0.85f, 0.90f, 1.00f };

// Agregar URL — Turquesa Profunda
static constexpr ImVec4 k_BtnTeal  = { 0.10f, 0.45f, 0.48f, 1.00f };
static constexpr ImVec4 k_BtnTealH = { 0.15f, 0.60f, 0.64f, 1.00f };
static constexpr ImVec4 k_BtnTealA = { 0.08f, 0.35f, 0.38f, 1.00f };
static constexpr ImVec4 k_BtnTealT = { 0.85f, 1.00f, 1.00f, 1.00f };
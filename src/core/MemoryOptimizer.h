#pragma once

namespace ProyecThor::Core {

// ─────────────────────────────────────────────────────────────────────────────
//  MemoryOptimizer
//  Centraliza la liberación segura de cachés en RAM/VRAM (miniaturas GPU,
//  índices de canciones, texturas y FBOs inactivos de proyección) y el recorte
//  del Working Set físico del proceso en el sistema operativo para equipos con
//  memoria mínima (2–4 GB de RAM).
// ─────────────────────────────────────────────────────────────────────────────
class MemoryOptimizer {
public:
    // Encola una limpieza para ejecutarse de forma segura al finalizar el frame
    // actual (después de ImGui_ImplOpenGL3_RenderDrawData y glfwSwapBuffers),
    // evitando borrar texturas OpenGL referenciadas por ImDrawData a mitad de frame.
    static void RequestPurge(bool purgeThumbnails = true, bool trimWorkingSet = true);

    // Ejecuta cualquier solicitud pendiente en un límite seguro entre frames.
    static void ProcessPending();

    // Libera cachés inmediatamente (solo invocar fuera de ImGui::NewFrame..RenderDrawData).
    static void PurgeAll(bool trimWorkingSet = true);

    // Recorta el Working Set del proceso en el SO sin destruir texturas activas.
    static void TrimWorkingSet();

    // Memoria física actual (Working Set / RSS) utilizada por ProyecThor en MB.
    static float GetProcessRamMB();
};

} // namespace ProyecThor::Core

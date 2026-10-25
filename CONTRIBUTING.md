# Guía de Contribución a ProyecThor
*(Contributing to ProyecThor)*

¡Gracias por tu interés en contribuir a ProyecThor! Para mantener la estabilidad en vivo, la coherencia arquitectónica y el rendimiento crítico del software, seguimos un proceso ordenado para todas las contribuciones.

---

## 1. Proceso antes de enviar código

1. **Coordinación previa en Discord**: Antes de implementar una gran característica o refactorización, únete a la comunidad de desarrollo en Discord y presenta tu propuesta.
2. **Revisión de Arquitectura**: Lee [`docs/ARCHITECTURE.md`](./docs/ARCHITECTURE.md) para comprender la separación unidireccional entre el Core, Media (libVLC/OpenGL) y la interfaz de usuario (ImGui).
3. **Licencias y SDKs**: Consulta [`docs/THIRD_PARTY_LICENSES.md`](./docs/THIRD_PARTY_LICENSES.md) si tu cambio involucra librerías de terceros.

---

## 2. Manifiesto y Política sobre el uso de Inteligencia Artificial

Si utilizas herramientas o asistentes de IA (LLMs, Copilot, etc.) en tu flujo de trabajo, **debes leer y cumplir obligatoriamente nuestro manifiesto**:

👉 **[Manifiesto y Política de Uso de IA (`docs/AI_POLICY.md`)](./docs/AI_POLICY.md)**

### Puntos clave:
- **La IA es solo una herramienta más del montón**: La idea inicial, la base, la arquitectura y el diseño deben ser concebidos y organizados por un ser humano.
- **Pruebas exhaustivas**: Todo código debe probarse en hardware real para garantizar cero caídas de frames, fugas de memoria o inestabilidades.
- **Tolerancia cero al AI Slop**: PRs con código desatendido, hinchado o no comprendido por el autor serán rechazados inmediatamente. Quien desee usar IA de forma irresponsable puede crear un fork independiente bajo la licencia MIT.

---

## 3. Pruebas y Compilación Local

Antes de abrir un Pull Request, asegúrate de que el proyecto compila limpiamente sin advertencias y se ejecuta sin errores:

```bash
# En Linux
cmake -B build-linux -G Ninja
ninja -C build-linux

# En Windows
cmake -B build-win -G "Visual Studio 17 2022" -A x64
cmake --build build-win --config Release
```

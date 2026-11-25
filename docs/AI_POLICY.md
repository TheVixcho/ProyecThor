# Manifiesto y Política de Uso de Inteligencia Artificial en ProyecThor
*(AI Contribution Policy & Manifesto)*

> **En pocas palabras:** En ProyecThor, la Inteligencia Artificial está permitida única y exclusivamente como **una herramienta más del montón** (como un linter, un depurador o un autocompletador avanzado). El software está diseñado para producción multimedia en vivo de alta exigencia, por lo que **la idea inicial, la arquitectura, la base y el criterio deben ser 100% humanos**, respaldados por pruebas exhaustivas de rendimiento y estabilidad en hardware real.

---

## 1. Contexto y Filosofía del Proyecto

ProyecThor es un motor de proyección y presentación en tiempo real utilizado en iglesias, teatros, conferencias y eventos en vivo. En este tipo de entornos, la estabilidad, la latencia mínima y la ausencia de cuelgues o caídas de cuadros (*frame drops*) son críticas: **un fallo en vivo interrumpe una ceremonia o producción frente a cientos de personas**.

Por esta razón, la calidad, limpieza y comprensión del código fuente no son negociables. El proyecto valora el ingenio humano, el diseño consciente y el control absoluto sobre cada llamada al sistema, capa gráfica y pipeline multimedia.

---

## 2. Reglas Obligatorias para Pull Requests y Contribuciones

Si utilizas asistentes de Inteligencia Artificial (LLMs, copilot, etc.) para colaborar en ProyecThor, tu contribución **debe cumplir obligatoriamente** con los siguientes principios:

### A. La Idea, Base y Organización Deben Ser Humanas
- **No se aceptan contribuciones donde la IA sea la autora intelectual de la solución.**
- La arquitectura, la propuesta de diseño, la estructura de carpetas y clases, y la lógica de fondo deben haber sido concebidas, decididas y planificadas por un desarrollador humano.
- La IA puede utilizarse para asistencia puntual: generar esqueletos repetitivos, redactar documentación interna, proponer optimizaciones matemáticas o ayudar a depurar errores de sintaxis, pero **nunca para diseñar la solución por ti**.

### B. Comprensión Total del Código Enviado
- Quien abre un Pull Request es **100% responsable** de cada carácter y línea de código introducida.
- Debes ser capaz de explicar y defender técnicamente por qué se tomó cada decisión en la revisión de código (*code review*).
- Si ante una pregunta en la revisión de código la respuesta denota que el autor no entiende qué hace el fragmento generado por IA o por qué está ahí, **el PR será cerrado inmediatamente**.

### C. Pruebas Exhaustivas de Rendimiento y Estabilidad
ProyecThor renderiza a 60 / 144 FPS con sincronización vertical, aceleración por hardware (OpenGL + libVLC) y consumo eficiente de memoria. Cualquier código asistido por IA debe someterse a:
1. **Pruebas de estrés y reproducción continua**: Verificar que no existan fugas de memoria (*memory leaks*), bloqueos de hilos (*deadlocks*) ni fugas de descriptores de archivos.
2. **Impacto en el bucle principal**: Prohibido introducir operaciones síncronas pesadas, I/O bloqueante o asignaciones excesivas en el ciclo de dibujado.
3. **Pruebas en hardware real**: El cambio debe haber sido probado localmente en Linux y/o Windows antes de proponerlo.

### D. Transparencia y Declaración
- Debes declarar abiertamente en la descripción del Pull Request si utilizaste asistencia de IA y en qué partes específicas te apoyó (siguiendo los estándares de transparencia del software libre y la política de *Generative AI* de Flathub).

---

## 3. Tolerancia Cero al "AI Slop" y Forks

Definimos como **"AI Slop"** al código basura generado masivamente por modelos de lenguaje: código hinchado (*bloatware*), alucinaciones de librerías, parches superficiales que no atacan la causa raíz de un problema, o implementaciones desatendidas generadas con un simple *prompt* sin revisión humana rigurosa.

> [!CAUTION]
> **No aceptamos AI Slop en este repositorio.**  
> Cualquier Pull Request que evidencie haber sido generado de forma descuidada, sin supervisión humana, sin pruebas en vivo o sin entendimiento técnico de la arquitectura de ProyecThor será **cerrado y rechazado sin contemplaciones**.

**¿Quieres experimentar o desarrollar con IA de forma desatendida o indiscriminada?**  
La licencia de ProyecThor es **MIT**: tienes total libertad legal de hacer un **Fork** del proyecto, clonar el repositorio y experimentar con IA como mejor te parezca en tu propia versión independiente. Pero en el repositorio oficial de ProyecThor, la estabilidad del operador en vivo siempre está primero.

---

## English Summary

- **AI is just a tool, not the author:** AI assistance is permitted strictly as an aid (like a compiler warning or syntax helper). The core idea, architectural foundation, and logic must be human-designed.
- **You own the code:** Contributors must fully understand, explain, and stand behind every single line of code submitted.
- **Mandatory performance & stability testing:** ProyecThor runs live productions. Code must be battle-tested against memory leaks, deadlocks, and frame drops on real hardware.
- **Zero tolerance for AI Slop:** Automated, unverified, or careless AI-generated pull requests will be closed immediately.
- **Respect the project or fork it:** If you wish to use AI carelessly, feel free to fork the repository under the MIT license and maintain your own separate build. The official upstream repo prioritizes rock-solid live stability.

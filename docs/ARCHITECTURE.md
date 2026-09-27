# Arquitectura de Software de ProyecThor

## 1. Filosofía y Principios de Diseño

ProyecThor es una aplicación de escritorio nativa multimedia de alto rendimiento desarrollada en **C++17** con renderizado acelerado por hardware mediante **OpenGL 3.3+**, **GLFW**, **Dear ImGui** y **libVLC**.

A diferencia de las arquitecturas cliente-servidor web (donde existe un servidor "backend" y un navegador "frontend" separados por red), en una aplicación de escritorio nativa todo el código se ejecuta dentro del mismo proceso del sistema operativo. Por esta razón, el código se organiza siguiendo una **Arquitectura en Capas Unidireccional** inspirada en los estándares de software multimedia como OBS Studio y Blender:

```
┌────────────────────────────────────────────────────────┐
│                   CAPA DE INTERFAZ (UI)                │
│       src/ui/ : ImGui, Paneles, Vistas, Framework      │
└───────────────────────────┬────────────────────────────┘
                            │ Depende de
                            ▼
┌────────────────────────────────────────────────────────┐
│             CAPA GRÁFICA / RENDER (Graphics)           │
│    src/graphics/ : Shaders OpenGL, Post-Process, FSR   │
└───────────────────────────┬────────────────────────────┘
                            │ Depende de
              ┌─────────────┴─────────────┐
              ▼                           ▼
┌───────────────────────────┐ ┌──────────────────────────┐
│    CAPA DE MEDIOS (Media) │ │ CAPA DE NEGOCIO (Core)   │
│ src/media/ : VLC, Audio,  │ │ src/core/ : Estado,      │
│ OS Screen Capture         │ │ Settings, IO, Red        │
└───────────────────────────┘ └──────────────────────────┘
              ▲                           ▲
              └─────────────┬─────────────┘
                            │ Depende de
                            ▼
┌────────────────────────────────────────────────────────┐
│          CAPA BASE / TERCEROS (Vendor / SDK)           │
│   src/vendor/ : STB, NanoSVG, QR, GLFW, GLEW, PDFium   │
└────────────────────────────────────────────────────────┘
```

---

## 2. La Regla de Oro del Dominio

> [!IMPORTANT]
> **El Core y Media NUNCA incluyen `imgui.h` ni código de interfaz gráfica.**
> La lógica de negocio, persistencia, parsing y motores de reproducción deben poder compilarse y ejecutarse de forma autónoma. Toda interacción entre la interfaz (UI) y el núcleo (Core/Media) ocurre mediante llamadas a métodos públicos, estructuras de datos limpias o callbacks/eventos.

---

## 3. Estructura Detallada de Directorios

```text
src/
├── core/                       # Lógica de negocio y estado puro
│   ├── settings/               # SettingsManager, AppSettings, presets de calidad
│   ├── io/                     # Lectores y parsers de datos: BibleXmlIO, DocumentConverter
│   ├── PresentationCore.*      # Estado de presentación y compositor central
│   ├── BackgroundLayer.*       # Capa de fondo y buffer de reproducción
│   ├── SyncServer.*            # Servidor HTTP/WebSocket de sincronización local
│   ├── ClaudeClient.*          # Cliente API para inteligencia artificial
│   ├── OSCReceiver.*           # Receptor de protocolo OSC (Open Sound Control)
│   ├── OSCSender.*             # Emisor de protocolo OSC
│   ├── PerformanceGovernor.*   # Control dinámico de framerate y carga de CPU
│   └── SystemStats.*           # Métricas de hardware y recursos del sistema
│
├── media/                      # Motores multimedia y drivers de hardware
│   ├── player/                 # VLCBasePlayer: motor LibVLC con render a textura
│   ├── audio/                  # AudioMixdown, AudioRecorder: motores de audio
│   └── capture/                # Capturadores de pantalla nativos de bajo nivel
│       ├── WaylandScreenCapture # Portal XDG Desktop + PipeWire + D-Bus (Linux)
│       └── Win32ScreenCapture   # DXGI Desktop Duplication (Windows)
│
├── graphics/                   # Pipeline gráfico y post-procesamiento
│   └── shaders/                # Shaders GLSL y post-procesadores de efectos:
│       ├── CompositePostChain  # Encadenador de efectos de composición
│       ├── PostProcessorFSR    # AMD FidelityFX Super Resolution 1.0
│       ├── PostProcessorNIS    # NVIDIA Image Scaling
│       ├── PostProcessorTAA    # Temporal Anti-Aliasing
│       ├── PostProcessorBloom  # Efecto de resplandor (Bloom)
│       └── PostProcessor*      # Filtros CRT, VHS, Blur, Vignette, etc.
│
├── ui/                         # Interfaz gráfica de usuario (Dear ImGui)
│   ├── framework/              # Núcleo del sistema de diseño y componentes UI
│   │   ├── DesignSystem.*      # Tokens de color, espaciado y estilos visuales
│   │   ├── UIManager.*         # Orquestador del ciclo de renderizado de UI
│   │   ├── GlassRenderer.*     # Efectos de vidrio esmerilado y sombras
│   │   ├── Hub.*               # Barra superior / barra central de navegación
│   │   ├── AppIcons.h          # Glifos y renderizado de iconos
│   │   ├── IconRail.*          # Riel de navegación rápida
│   │   ├── FilePicker.*        # Selector de archivos embebido
│   │   ├── LoadingSpinner.*    # Indicador animado de carga
│   │   └── lenguaje/           # Internacionalización (es, en, pt)
│   ├── windowing/              # Gestión de ventanas multi-monitor GLFW
│   │   ├── NativeVideoOutputWindow # Salida de video acelerada a pantalla completa
│   │   └── SecondaryOutputWindow   # Ventana secundaria de proyección
│   ├── panels/                 # Paneles desacoplados:
│   │   ├── settings/           # SettingsPanel y sus pestañas (CategoryAudio, etc.)
│   │   ├── monitor/            # Vistas previas de monitores, vúmetros, cola
│   │   ├── biblio/             # Explorador de biblioteca y multimedia
│   │   ├── capture/            # Panel de control de capturas de pantalla/cámara
│   │   ├── overlay/            # Editor de capas y plantillas gráficas
│   │   ├── styles/             # Editor de estilos tipográficos y canvas
│   │   ├── layers/             # Gestión de capas de fondo y transiciones
│   │   ├── home/               # Pantalla de inicio y bienvenida
│   │   ├── model3d/            # Visor y cargador de modelos 3D
│   │   └── lab/                # Panel de experimentos y utilidades
│   ├── views/                  # Vistas de proyección de contenido:
│   │   ├── songs/              # SongView, SongEditView
│   │   ├── biblia/             # BibleView, BibleSearch, BibleQuickNav
│   │   ├── audio/              # Reproductor de audio, formas de onda y portadas
│   │   ├── ImageView.*         # Visualizador de imágenes fijas
│   │   ├── DocumentView.*      # Visualizador de presentaciones y PDFs
│   │   ├── MediaView.*         # Visualizador de video y fuentes multimedia
│   │   └── OClock.*            # Reloj y temporizador de escenario
│   └── toolbar/                # Barras de herramientas de acceso rápido
│
├── vendor/                     # Librerías externas vendorizadas
│   ├── stb/                    # STB Image (carga, escritura y redimensionado)
│   ├── nanosvg/                # NanoSVG (rasterizado vectorial ligero)
│   ├── qrcodegen/              # Generador de códigos QR en C++
│   └── sdk/                    # SDKs binarios externos (GLFW, GLEW, VLC, PDFium)
│
└── main.cpp                    # Punto de entrada de la aplicación
```

---

## 4. Guía para Nuevas Características

Al agregar nuevas funcionalidades a ProyecThor, ubica los archivos según la siguiente tabla:

| Tipo de Contenido | Ubicación Correcta | Ejemplo |
| :--- | :--- | :--- |
| **Nuevo formato de archivo o parser** | `src/core/io/` | `src/core/io/EpubReader.cpp` |
| **Nuevo modelo de datos o estado** | `src/core/` o `src/core/settings/` | `src/core/PlaylistManager.cpp` |
| **Nuevo motor de captura o hardware** | `src/media/capture/` o `src/media/player/` | `src/media/capture/NDICapture.cpp` |
| **Nuevo shader o filtro visual** | `src/graphics/shaders/` | `src/graphics/shaders/PostProcessorLens.cpp` |
| **Nueva ventana o pestaña de interfaz** | `src/ui/panels/` | `src/ui/panels/LightingControlPanel.cpp` |
| **Nuevo tipo de presentación/vista** | `src/ui/views/` | `src/ui/views/KeynoteView.cpp` |
| **Librería externa single-header** | `src/vendor/nombre/` | `src/vendor/miniaudio/` |

---

## 5. Convenciones de Include

CMake está configurado para permitir rutas absolutas relativas a `src/`:

```cpp
// Correcto: Rutas claras que identifican el módulo
#include "core/PresentationCore.h"
#include "core/settings/SettingsManager.h"
#include "media/player/VLCBasePlayer.h"
#include "graphics/shaders/CompositePostChain.h"
#include "ui/framework/DesignSystem.h"
#include "ui/panels/ViewPanel.h"
#include "vendor/stb/stb_image.h"
```

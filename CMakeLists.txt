cmake_minimum_required(VERSION 3.11)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project(ProyecThor
    VERSION 1.0.1
    LANGUAGES C CXX
)

# Sin esto, CMake no aplica ninguna optimizacion por defecto (equivale a
# -O0): el binario resultante corre notablemente mas lento en PCs de bajos
# recursos. Quien necesite un build de debug puede seguir pasando
# -DCMAKE_BUILD_TYPE=Debug explicitamente.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O2)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# ── CONTADOR DE BUILDS ────────────────────────────────────────────────────────
# Aumenta SOLO al reconfigurar con CMake (compilar de verdad: "cmake -B" a
# mano, o un build limpio de CI), nunca al abrir la app -- antes se
# incrementaba en cada arranque (ver splash screen / GetAndIncrementBuildCount
# en main.cpp, retirado), lo que en la practica contaba "veces que se probo
# el build" en vez de "veces que se compilo". CI sigue pudiendo fijar su
# propio numero pasando -DPROYECTHOR_BUILD_NUMBER=N (ver
# .github/workflows/build.yml), en cuyo caso este contador local no se toca.
if(NOT DEFINED PROYECTHOR_BUILD_NUMBER)
    set(_build_count_file "${CMAKE_CURRENT_SOURCE_DIR}/build_count.txt")
    set(PROYECTHOR_BUILD_NUMBER 0)
    if(EXISTS "${_build_count_file}")
        file(READ "${_build_count_file}" PROYECTHOR_BUILD_NUMBER)
        string(STRIP "${PROYECTHOR_BUILD_NUMBER}" PROYECTHOR_BUILD_NUMBER)
    endif()
    math(EXPR PROYECTHOR_BUILD_NUMBER "${PROYECTHOR_BUILD_NUMBER} + 1")
    file(WRITE "${_build_count_file}" "${PROYECTHOR_BUILD_NUMBER}")
endif()

# ── SUFIJO DE VERSION (pre-release) ──────────────────────────────────────────
# Vacio = release estable ("1.0.0"). Para una beta/rc que todavia no es la
# version final, pasar -DPROYECTHOR_VERSION_SUFFIX=-beta.2 al configurar (o
# cambiar el default de aca a mano). IMPORTANTE: volver a "" ANTES de compilar
# la 1.0.0 estable de verdad, sino queda marcada como beta para siempre.
# project(VERSION ...) arriba no puede llevar el sufijo (CMake solo acepta
# numeros en esa directiva), por eso se maneja aparte y se concatena a mano en
# Version.h.in. El comparador de versiones del updater (IsNewer, ver
# CategoryUpdates.cpp) ya entiende sufijos -beta.N / -rc.N.
if(NOT DEFINED PROYECTHOR_VERSION_SUFFIX)
    set(PROYECTHOR_VERSION_SUFFIX "")
endif()

# ── VERSION GENERADA AUTOMATICAMENTE ─────────────────────────────────────────
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Version.h.in"
    "${CMAKE_BINARY_DIR}/generated/Version.h"
)

# ── 1. DEPENDENCIAS EXTERNAS (JSON, GLM y TagLib) ────────────────────────────

include(FetchContent)

# -- JSON --
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# -- GLM --
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 0.9.9.8
)
FetchContent_MakeAvailable(glm)

add_compile_definitions(IMGUI_USE_WCHAR32)

# ── 2. RUTAS DE LIBRERIAS ────────────────────────────────────────────────────
set(IMGUI_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/src/external/imgui-docking")
set(GLFW_SDK   "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sdk/glfw")
set(GLEW_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sdk/glew")
set(VLC_SDK    "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sdk/vlc")
set(VLC_SDK_LINUX "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sdk/vlc-linux")
set(PDFIUM_SDK "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sdk/pdfium")
set(PDFIUM_SDK_LINUX "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sdk/pdfium-linux")
# WebView2 (Asistente de IA, panel embebido -- ver AIWebViewPanel): headers +
# import lib (.dll.lib) vendorizados desde el paquete NuGet oficial de
# Microsoft (Microsoft.Web.WebView2). Solo Windows -- no existe equivalente
# de WebView2 en Linux, ahi el panel del Asistente cae a abrir el navegador
# externo del sistema (ver OpenURL.cpp).
set(WEBVIEW2_SDK "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sdk/webview2")
set(HTTPLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sdk")

# ── 3. ARCHIVOS FUENTE ───────────────────────────────────────────────────────

# -- CORE --
file(GLOB CORE_SOURCES              CONFIGURE_DEPENDS "src/core/*.cpp")
file(GLOB CORE_IO_SOURCES           CONFIGURE_DEPENDS "src/core/io/*.cpp")
file(GLOB CORE_SETTINGS_SOURCES     CONFIGURE_DEPENDS "src/core/settings/*.cpp")

# -- MEDIA --
file(GLOB MEDIA_PLAYER_SOURCES      CONFIGURE_DEPENDS "src/media/player/*.cpp")
file(GLOB MEDIA_AUDIO_SOURCES       CONFIGURE_DEPENDS "src/media/audio/*.cpp")
file(GLOB MEDIA_CAPTURE_SOURCES     CONFIGURE_DEPENDS "src/media/capture/*.cpp")

# -- GRAPHICS --
file(GLOB GRAPHICS_SHADERS_SOURCES  CONFIGURE_DEPENDS "src/graphics/shaders/*.cpp")
file(GLOB EXTERNAL_TOOLS            CONFIGURE_DEPENDS "src/external/tools/*.cpp")

# -- UI --
file(GLOB UI_FRAMEWORK_BIN_SOURCES   CONFIGURE_DEPENDS "src/ui/framework/bin/*.cpp")
file(GLOB UI_FRAMEWORK_SOURCES       CONFIGURE_DEPENDS "src/ui/framework/*.cpp")
file(GLOB UI_LANG_SOURCES            CONFIGURE_DEPENDS "src/ui/framework/lenguaje/*.cpp")
file(GLOB UI_WINDOWING_SOURCES       CONFIGURE_DEPENDS "src/ui/windowing/*.cpp")
file(GLOB UI_PANELS_SOURCES          CONFIGURE_DEPENDS "src/ui/panels/*.cpp")
file(GLOB UI_SETTINGS_SOURCES        CONFIGURE_DEPENDS "src/ui/panels/settings/*.cpp")
file(GLOB UI_CAPTURE_SOURCES         CONFIGURE_DEPENDS "src/ui/panels/capture/*.cpp")
file(GLOB UI_LAYERS_SOURCES          CONFIGURE_DEPENDS "src/ui/panels/layers/*.cpp")
file(GLOB UI_OVERLAY_SOURCES         CONFIGURE_DEPENDS "src/ui/panels/overlay/*.cpp")
file(GLOB UI_MONITOR_SOURCES         CONFIGURE_DEPENDS "src/ui/panels/monitor/*.cpp")
file(GLOB UI_STYLES_SOURCES          CONFIGURE_DEPENDS "src/ui/panels/styles/*.cpp")
file(GLOB UI_BIBLIO_SOURCES          CONFIGURE_DEPENDS "src/ui/panels/biblio/*.cpp")
file(GLOB UI_HOME_SOURCES            CONFIGURE_DEPENDS "src/ui/panels/home/*.cpp")
file(GLOB UI_MODEL3D_SOURCES         CONFIGURE_DEPENDS "src/ui/panels/model3d/*.cpp")
file(GLOB UI_LAB_SOURCES             CONFIGURE_DEPENDS "src/ui/panels/lab/*.cpp")
file(GLOB UI_VIEWS_SOURCES           CONFIGURE_DEPENDS "src/ui/views/*.cpp")
file(GLOB UI_VIEWS_BIBLIA_SOURCES    CONFIGURE_DEPENDS "src/ui/views/biblia/*.cpp")
file(GLOB UI_VIEWS_AUDIO_SOURCES     CONFIGURE_DEPENDS "src/ui/views/audio/*.cpp")

# -- VENDOR --
set(VENDOR_SOURCES
    "src/vendor/stb/stb_image_write_impl.cpp"
    "src/vendor/qrcodegen/qrcodegen.cpp"
)

# -- RAIZ src/ (main.cpp y similares) --
file(GLOB ROOT_SOURCES              CONFIGURE_DEPENDS "src/*.cpp")

# -- IMGUI --
set(IMGUI_SOURCES
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)

# -- RECURSO WINDOWS (solo aplica en Windows) --
if(WIN32)
    set(RESOURCE_FILE "src/resources.rc")
    set(CMAKE_RC_FLAGS "-O coff --use-temp-file")
else()
    set(RESOURCE_FILE "")
endif()

# --- COPIA DE RECURSOS AL DIRECTORIO BUILD ---
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/shaders"
     DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

if(WIN32 AND EXISTS "${PDFIUM_SDK}/bin/pdfium.dll")
    file(COPY "${PDFIUM_SDK}/bin/pdfium.dll"
         DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
endif()

# ── 4. CREACION DEL EJECUTABLE ───────────────────────────────────────────────
# En Windows el ejecutable debe ser WIN32 (subsistema grafico, sin consola).
# En Linux no existe ese concepto, asi que se crea como ejecutable normal.
if(WIN32)
    add_executable(ProyecThor WIN32
        ${ROOT_SOURCES}
        ${CORE_SOURCES}
        ${CORE_SETTINGS_SOURCES}
        ${CORE_IO_SOURCES}
        ${MEDIA_PLAYER_SOURCES}
        ${MEDIA_AUDIO_SOURCES}
        ${MEDIA_CAPTURE_SOURCES}
        ${GRAPHICS_SHADERS_SOURCES}
        ${UI_FRAMEWORK_SOURCES}
        ${UI_FRAMEWORK_BIN_SOURCES}
        ${UI_LANG_SOURCES}
        ${UI_WINDOWING_SOURCES}
        ${UI_PANELS_SOURCES}
        ${UI_SETTINGS_SOURCES}
        ${UI_CAPTURE_SOURCES}
        ${UI_LAYERS_SOURCES}
        ${UI_OVERLAY_SOURCES}
        ${UI_MONITOR_SOURCES}
        ${UI_STYLES_SOURCES}
        ${UI_BIBLIO_SOURCES}
        ${UI_HOME_SOURCES}
        ${UI_MODEL3D_SOURCES}
        ${UI_LAB_SOURCES}
        ${UI_VIEWS_SOURCES}
        ${UI_VIEWS_BIBLIA_SOURCES}
        ${UI_VIEWS_AUDIO_SOURCES}
        ${VENDOR_SOURCES}
        ${EXTERNAL_TOOLS}
        ${IMGUI_SOURCES}
        ${RESOURCE_FILE}
        "${GLEW_DIR}/src/glew.c"
    )
else()
    add_executable(ProyecThor
        ${ROOT_SOURCES}
        ${CORE_SOURCES}
        ${CORE_SETTINGS_SOURCES}
        ${CORE_IO_SOURCES}
        ${MEDIA_PLAYER_SOURCES}
        ${MEDIA_AUDIO_SOURCES}
        ${MEDIA_CAPTURE_SOURCES}
        ${GRAPHICS_SHADERS_SOURCES}
        ${UI_FRAMEWORK_SOURCES}
        ${UI_FRAMEWORK_BIN_SOURCES}
        ${UI_LANG_SOURCES}
        ${UI_WINDOWING_SOURCES}
        ${UI_PANELS_SOURCES}
        ${UI_SETTINGS_SOURCES}
        ${UI_CAPTURE_SOURCES}
        ${UI_LAYERS_SOURCES}
        ${UI_OVERLAY_SOURCES}
        ${UI_MONITOR_SOURCES}
        ${UI_STYLES_SOURCES}
        ${UI_BIBLIO_SOURCES}
        ${UI_HOME_SOURCES}
        ${UI_MODEL3D_SOURCES}
        ${UI_LAB_SOURCES}
        ${UI_VIEWS_SOURCES}
        ${UI_VIEWS_BIBLIA_SOURCES}
        ${UI_VIEWS_AUDIO_SOURCES}
        ${VENDOR_SOURCES}
        ${EXTERNAL_TOOLS}
        ${IMGUI_SOURCES}
        "${GLEW_DIR}/src/glew.c"
    )
endif()

# ── 5. DEFINICIONES Y DIRECTORIOS DE INCLUDE ─────────────────────────────────
target_compile_definitions(ProyecThor PRIVATE
    GLEW_STATIC
    GLEW_NO_GLU
    GLFW_INCLUDE_NONE
    CPPHTTPLIB_NO_EXCEPTIONS
)

# OpenSSL para httplib::Client saliente por HTTPS (api.anthropic.com, ver
# AIAssistant/ClaudeClient) -- todo lo que ya usaba httplib en este proyecto
# (SyncServer, NetworkStreamServer, single-instance de main.cpp) era HTTP
# plano contra localhost, asi que nunca hizo falta SSL hasta ahora. En
# Windows es obligatorio (el toolchain de este proyecto es MSYS2/ucrt64,
# que ya trae libssl/libcrypto); en Linux es opcional con fallback
# silencioso (mismo criterio que OpenCV mas abajo) -- sin libssl-dev
# instalado, el asistente de IA simplemente no puede conectarse (se avisa
# en tiempo de ejecucion, no en compilacion).
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    message(STATUS "OpenSSL encontrado (${OPENSSL_VERSION}): Asistente de IA (HTTPS saliente) habilitado")
    target_compile_definitions(ProyecThor PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
    set(PROYECTHOR_HAS_OPENSSL TRUE)
else()
    message(STATUS "OpenSSL NO encontrado: el Asistente de IA no podra conectarse")
    set(PROYECTHOR_HAS_OPENSSL FALSE)
endif()

if(WIN32)
    target_compile_definitions(ProyecThor PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
endif()

target_include_directories(ProyecThor PRIVATE
    # Version generada
    "${CMAKE_BINARY_DIR}/generated"

    # Raiz src/ para includes absolutos desde src/
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    # CORE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/settings"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/io"

    # GRAPHICS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/shaders"

    # UI
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/framework"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/framework/bin"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/windowing"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/panels"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/panels/settings"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/panels/capture"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/panels/layers"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/panels/overlay"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/panels/monitor"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/panels/styles"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/panels/biblio"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/panels/home"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/panels/model3d"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/panels/lab"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/views"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/views/biblia"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/views/audio"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/toolbar"

    # MEDIA
    "${CMAKE_CURRENT_SOURCE_DIR}/src/media"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/media/player"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/media/audio"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/media/capture"

    # VENDOR / TERCEROS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/stb"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/nanosvg"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/qrcodegen"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sdk"

    # EXTERNAS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/external/tools"
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
    "${GLFW_SDK}/include"
    "${GLEW_DIR}/include"
    "${VLC_SDK}/include"

    # httplib — está directamente en src/vendor/sdk/httplib.h
    "${HTTPLIB_DIR}"
    # BIN
    "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)

# El include de PDFium y VLC cambia segun la plataforma
if(WIN32)
    target_include_directories(ProyecThor PRIVATE "${PDFIUM_SDK}/include" "${WEBVIEW2_SDK}/include")
else()
    target_include_directories(ProyecThor PRIVATE
        "${PDFIUM_SDK_LINUX}/include"
        "${VLC_SDK_LINUX}/include"
    )
endif()

# ── 6. LINKEO DE LIBRERIAS ───────────────────────────────────────────────────

target_link_libraries(ProyecThor PRIVATE
    nlohmann_json::nlohmann_json
    glm::glm
)

if(WIN32)
    # ── Rutas y librerias exclusivas de Windows ──────────────────────────────
    set(PDFIUM_LIB "${PDFIUM_SDK}/lib/pdfium.dll.lib")

    if(NOT EXISTS "${PDFIUM_LIB}")
        message(FATAL_ERROR
            "No se encontro pdfium.dll.lib en ${PDFIUM_SDK}/lib\n"
            "Verifica que el archivo existe en: ${PDFIUM_LIB}"
        )
    endif()

    message(STATUS "PDFium encontrado (Windows): ${PDFIUM_LIB}")

    target_link_libraries(ProyecThor PRIVATE
        "${GLFW_SDK}/lib-mingw-w64/libglfw3.a"
        "${VLC_SDK}/lib/libvlc.lib"
        "${VLC_SDK}/lib/libvlccore.lib"
        "${PDFIUM_LIB}"
        "${WEBVIEW2_SDK}/x64/WebView2Loader.dll.lib"
        opengl32
        gdi32
        shell32
        dwmapi
        comdlg32
        winmm
        ws2_32
        iphlpapi
        winhttp
        d3d11
        dxgi
    )
    if(PROYECTHOR_HAS_OPENSSL)
        target_link_libraries(ProyecThor PRIVATE
            ssl
            crypto
            crypt32
            bcrypt
            ws2_32
        )
    endif()

    target_link_options(ProyecThor PRIVATE
        "-static"
    )

else()
    # ── Rutas y librerias exclusivas de Linux ────────────────────────────────

    # GLFW se compila desde fuente (FetchContent) en vez de usar el paquete
    # del sistema. Motivo: el backend de ImGui vendorizado (ver
    # src/external/imgui-docking/backends/imgui_impl_glfw.cpp) llama a
    # glfwGetPlatform() -- la API que permite elegir/detectar X11 vs Wayland
    # en tiempo de ejecucion -- que recien existe desde GLFW 3.4. Debian/
    # Ubuntu todavia empaquetan GLFW 3.3.x ("libglfw3-dev"), asi que linkear
    # contra esa version tira "undefined reference a glfwGetPlatform" al
    # linkear (confirmado en CI, ver build-linux). Se compilan ambos
    # backends (X11 + Wayland) para que el mismo binario funcione en
    # cualquiera de los dos sin recompilar -- necesita los paquetes de
    # desarrollo de Wayland instalados ademas de los de X11 (ver
    # .github/workflows/build.yml).
    include(FetchContent)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_X11      ON  CACHE BOOL "" FORCE)
    set(GLFW_BUILD_WAYLAND  ON  CACHE BOOL "" FORCE)
    FetchContent_Declare(
        glfw3_src
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(glfw3_src)
    # De aca en mas "glfw" es el target que provee FetchContent -- mismo
    # nombre que exportaba find_package(glfw3), el resto del archivo
    # (target_link_libraries mas abajo) no necesita cambios.

    find_package(OpenGL REQUIRED)

    # Camaras reales en el panel de Captura: CapturePanel.cpp ya tiene la
    # enumeracion/lectura real via cv::VideoCapture (V4L2 por debajo) detras
    # de la macro PT_USE_OPENCV -- pero como nada definia esa macro nunca se
    # compilaba, y el panel caia siempre en el stub de "Camara 0 (simulada)"
    # con un patron de prueba animado en vez de listar dispositivos reales.
    # Opcional, igual que XComposite/PipeWire arriba/abajo: si OpenCV no
    # esta instalado, el build sigue igual que antes (stub simulado).
    find_package(OpenCV QUIET COMPONENTS core videoio)
    if(OpenCV_FOUND)
        message(STATUS "OpenCV encontrado (${OpenCV_VERSION}): deteccion real de camaras habilitada en Captura")
        target_link_libraries(ProyecThor PRIVATE ${OpenCV_LIBS})
        target_include_directories(ProyecThor PRIVATE ${OpenCV_INCLUDE_DIRS})
        target_compile_definitions(ProyecThor PRIVATE PT_USE_OPENCV)
    else()
        message(STATUS "OpenCV NO encontrado: Captura seguira usando la camara simulada de prueba")
    endif()

    # VLC para Linux: build .so vendorizado, igual que PDFium (no se
    # compila VLC desde cero, se linkea contra binarios ya generados en
    # src/backend/sdk/vlc-linux/lib/ y src/backend/sdk/vlc-linux/include/)
    set(VLC_LIB_LINUX "${VLC_SDK_LINUX}/lib/libvlc.so")

    if(NOT EXISTS "${VLC_LIB_LINUX}")
        message(FATAL_ERROR
            "No se encontro libvlc.so en ${VLC_SDK_LINUX}/lib\n"
            "Coloca ahi los .so de VLC (libvlc.so, libvlccore.so),\n"
            "la carpeta plugins/ y sus headers en:\n"
            "${VLC_SDK_LINUX}/include"
        )
    endif()

    message(STATUS "libVLC encontrado (Linux): ${VLC_LIB_LINUX}")

    # PDFium para Linux: debe existir un build .so colocado manualmente
    set(PDFIUM_LIB_LINUX "${PDFIUM_SDK_LINUX}/lib/libpdfium.so")

    if(NOT EXISTS "${PDFIUM_LIB_LINUX}")
        message(FATAL_ERROR
            "No se encontro libpdfium.so en ${PDFIUM_SDK_LINUX}/lib\n"
            "Descarga el build de PDFium para Linux y colocalo en:\n"
            "${PDFIUM_SDK_LINUX}/lib/libpdfium.so\n"
            "Y sus headers en:\n"
            "${PDFIUM_SDK_LINUX}/include"
        )
    endif()

    message(STATUS "PDFium encontrado (Linux): ${PDFIUM_LIB_LINUX}")

    target_link_libraries(ProyecThor PRIVATE
        glfw
        OpenGL::GL
        "${VLC_LIB_LINUX}"
        "${PDFIUM_LIB_LINUX}"
        pthread
        dl
    )

    if(PROYECTHOR_HAS_OPENSSL)
        target_link_libraries(ProyecThor PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    endif()
endif()

# ── 7. COPIA DE ARCHIVOS EN RUNTIME (solo Windows) ───────────────────────────

if(WIN32)
    # Copiar DLLs de VLC
    if(EXISTS "${VLC_SDK}/libvlc.dll")
        add_custom_command(TARGET ProyecThor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VLC_SDK}/libvlc.dll"
                "$<TARGET_FILE_DIR:ProyecThor>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VLC_SDK}/libvlccore.dll"
                "$<TARGET_FILE_DIR:ProyecThor>"
            COMMENT "Copiando DLLs de VLC al directorio de salida"
        )
    endif()

    # Copiar carpeta plugins de VLC (necesaria en runtime)
    if(EXISTS "${VLC_SDK}/plugins")
        add_custom_command(TARGET ProyecThor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${VLC_SDK}/plugins"
                "$<TARGET_FILE_DIR:ProyecThor>/plugins"
            COMMENT "Copiando plugins de VLC"
        )
    endif()

    # WebView2Loader.dll: trampolin liviano que en runtime busca el Runtime
    # de WebView2 real instalado en el sistema (viene con Windows 10/11 via
    # Edge) -- sin esta DLL al lado del .exe, CreateCoreWebView2EnvironmentWithOptions
    # falla siempre con HRESULT de "modulo no encontrado".
    if(EXISTS "${WEBVIEW2_SDK}/x64/WebView2Loader.dll")
        add_custom_command(TARGET ProyecThor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${WEBVIEW2_SDK}/x64/WebView2Loader.dll"
                "$<TARGET_FILE_DIR:ProyecThor>"
            COMMENT "Copiando WebView2Loader.dll al directorio de salida"
        )
    endif()
endif()

# ── 7a-bis. COPIA DE assets/ AL DIRECTORIO DE SALIDA (Windows y Linux) ───────
# A diferencia de extrabuild/ (gitignored, binarios vendorizados pesados:
# libvlc.dll, plugins/ de VLC, ffmpeg.exe, yt-dlp.exe -- Linux ni los
# necesita, usa las versiones del sistema), assets/ SI esta en git: son los
# recursos minimos multiplataforma sin los que la app arranca "pelada" --
# iconos, fuente, texturas de tarjetas del Hub y fondos de splash/Hub (estos
# ultimos re-comprimidos a JPG, ver README de la carpeta). Quien clona desde
# GitHub y compila ya los tiene, sin depender de extrabuild/.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    add_custom_command(TARGET ProyecThor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:ProyecThor>"
        COMMENT "Copiando assets/ al directorio de salida"
    )
else()
    message(WARNING "assets/ NO encontrado en ${CMAKE_CURRENT_SOURCE_DIR}/assets -- "
                     "la app va a arrancar sin iconos/fuente/splash")
endif()

# ── 7b. COPIA DE extrabuild/ AL DIRECTORIO DE SALIDA (Windows y Linux) ───────
# Contiene binarios/recursos pesados que se distribuyen aparte, no en git
# (bin/, lua/, plugins/, libvlc.dll, pdfium.dll, yt-dlp.exe, etc -- ver
# extrabuild/pasteinbuild.txt). Se copia entero al lado del ejecutable cada
# vez que se compila, ENCIMA de assets/ (mismo directorio de salida) -- no
# hay conflicto real porque ya no comparten archivos (fuente/iconos/splash
# se movieron a assets/, ver arriba).
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extrabuild")
    message(STATUS "extrabuild/ encontrado, se copiara al build")
    add_custom_command(TARGET ProyecThor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/extrabuild"
            "$<TARGET_FILE_DIR:ProyecThor>"
        COMMENT "Copiando extrabuild/ al directorio de salida"
    )
else()
    message(STATUS "extrabuild/ no aplica o no encontrado en Linux -- se usan las librerias y plugins nativos.")
endif()

# ── 7c. COPIA DE libpdfium.so Y libvlc.so AL DIRECTORIO DE BUILD (Linux) ─────
# Sin esto, el ejecutable de build-linux/ no encuentra las librerias hasta
# que se hace "install" (que si las copia a /usr/lib/proyecthor).
if(NOT WIN32)
    file(GLOB VLC_LINUX_SO_FILES "${VLC_SDK_LINUX}/lib/lib*.so*")
    add_custom_command(TARGET ProyecThor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PDFIUM_SDK_LINUX}/lib/libpdfium.so"
            "$<TARGET_FILE_DIR:ProyecThor>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${VLC_LINUX_SO_FILES}
            "$<TARGET_FILE_DIR:ProyecThor>"
        COMMENT "Copiando libpdfium.so y librerias de VLC al directorio de build"
    )

    if(EXISTS "${VLC_SDK_LINUX}/plugins")
        add_custom_command(TARGET ProyecThor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${VLC_SDK_LINUX}/plugins"
                "$<TARGET_FILE_DIR:ProyecThor>/plugins"
            COMMENT "Copiando plugins de VLC (Linux) al directorio de build"
        )
    endif()
endif()


if(NOT WIN32)
    include(GNUInstallDirs)

    set(PROYECTHOR_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/proyecthor")

    # RPATH relativo al propio ejecutable: encuentra libpdfium.so/libvlc.so
    # en la misma carpeta sin necesidad de LD_LIBRARY_PATH.
    set_target_properties(ProyecThor PROPERTIES
        INSTALL_RPATH "$ORIGIN"
        BUILD_WITH_INSTALL_RPATH TRUE
        SKIP_BUILD_RPATH FALSE
    )

    # Binario real (no directamente en PATH, lo llama el wrapper de abajo)
    install(TARGETS ProyecThor
        RUNTIME DESTINATION ${PROYECTHOR_INSTALL_DIR}
    )

    # libpdfium.so vive al lado del binario
    install(FILES "${PDFIUM_SDK_LINUX}/lib/libpdfium.so"
        DESTINATION ${PROYECTHOR_INSTALL_DIR}
    )

    # libvlc.so y libvlccore.so viven al lado del binario
    file(GLOB VLC_LINUX_SO_FILES "${VLC_SDK_LINUX}/lib/lib*.so*")
    install(FILES ${VLC_LINUX_SO_FILES}
        DESTINATION ${PROYECTHOR_INSTALL_DIR}
    )

    # Plugins de VLC (necesarios en runtime via VLC_PLUGIN_PATH)
    if(EXISTS "${VLC_SDK_LINUX}/plugins")
        install(DIRECTORY "${VLC_SDK_LINUX}/plugins"
            DESTINATION ${PROYECTHOR_INSTALL_DIR}
        )
    endif()

    # Shaders (misma carpeta que ya se copia al lado del binario en build/)
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders"
        DESTINATION ${PROYECTHOR_INSTALL_DIR}
    )

    # Wrapper ejecutable en /usr/bin: hace cd a la carpeta real, exporta
    # VLC_PLUGIN_PATH (para que libvlc encuentre sus plugins) y ejecuta el
    # binario desde ahi, para que las rutas relativas (shaders/, etc) sigan
    # resolviendo igual que en desarrollo, sin importar el cwd del usuario.
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/proyecthor-wrapper.sh"
"#!/bin/sh
cd \"${CMAKE_INSTALL_PREFIX}/${PROYECTHOR_INSTALL_DIR}\" || exit 1
export VLC_PLUGIN_PATH=\"${CMAKE_INSTALL_PREFIX}/${PROYECTHOR_INSTALL_DIR}/plugins\"
exec ./ProyecThor \"\$@\"
")
    install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/proyecthor-wrapper.sh"
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        RENAME proyecthor
    )

    # Icono y entrada de menu (.desktop) — opcionales, solo si existen los
    # archivos en packaging/. Si todavia no tenes un icono, comenta estas
    # dos lineas de install() de abajo.

    # assets/ (icono, fuente, splash/Hub, texturas de tarjetas) -- esta en
    # git, SIEMPRE presente para quien compila el paquete desde el repo
    # clonado. Es lo minimo para que el paquete no arranque "pelado".
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
        install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/"
            DESTINATION ${PROYECTHOR_INSTALL_DIR}
        )
    else()
        message(WARNING "assets/ NO encontrado, el paquete no tendra iconos/fuente/splash")
    endif()

    # Recursos de extrabuild/ (lua/, plugins/ de VLC, etc) -- gitignored,
    # opcional en Linux: el paquete .deb/.rpm depende de la libvlc/plugins
    # DEL SISTEMA (ver Depends en packaging/debian/control), no de esto.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extrabuild")
        message(STATUS "extrabuild/ encontrado, se incluira en el paquete")
        install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/extrabuild/"
            DESTINATION ${PROYECTHOR_INSTALL_DIR}
            PATTERN "*.dll" EXCLUDE
            PATTERN "*.exe" EXCLUDE
            PATTERN "pasteinbuild.txt" EXCLUDE
        )
    endif()
if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    target_link_libraries(ProyecThor PRIVATE ${X11_LIBRARIES})
    target_include_directories(ProyecThor PRIVATE ${X11_INCLUDE_DIR})

    # XComposite: opcional. Sin ella, la captura de ventanas (CapturePanel)
    # lee XGetImage directo sobre la ventana, que devuelve contenido
    # indefinido/basura en las zonas tapadas por otras ventanas. Si esta
    # disponible, la usamos para leer el pixmap ya compuesto por el
    # servidor X y capturar bien aunque la ventana este solapada. Si no
    # esta instalada (dev headers ausentes), el build sigue igual que
    # antes — no es un requisito duro.
    if(X11_Xcomposite_FOUND)
        message(STATUS "libXcomposite encontrada: captura de ventanas correcta bajo solapamiento")
        target_link_libraries(ProyecThor PRIVATE ${X11_Xcomposite_LIB})
        target_include_directories(ProyecThor PRIVATE ${X11_Xcomposite_INCLUDE_PATH})
        target_compile_definitions(ProyecThor PRIVATE PT_HAVE_XCOMPOSITE)
    else()
        message(STATUS "libXcomposite NO encontrada: captura de ventanas usara XGetImage directo (puede fallar si hay solapamiento)")
    endif()

    # Captura de pantalla/ventana en sesiones Wayland: XGetImage/XComposite
    # (arriba) no sirven ahi -- el compositor Wayland no vuelca contenido
    # real a la "ventana raiz" X11 heredada, es una restriccion de
    # seguridad del protocolo. El camino nativo es el portal de escritorio
    # (org.freedesktop.portal.ScreenCast) + PipeWire para consumir el
    # stream. Opcional: sin estas dos libs, el build sigue igual que hoy y
    # la captura de pantalla queda limitada a X11 puro.
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(PIPEWIRE libpipewire-0.3)
        pkg_check_modules(DBUS dbus-1)
    endif()
    if(PIPEWIRE_FOUND AND DBUS_FOUND)
        message(STATUS "libpipewire-0.3 y dbus-1 encontradas: captura de pantalla/ventana via portal Wayland habilitada")
        target_link_libraries(ProyecThor PRIVATE ${PIPEWIRE_LIBRARIES} ${DBUS_LIBRARIES})
        target_include_directories(ProyecThor PRIVATE ${PIPEWIRE_INCLUDE_DIRS} ${DBUS_INCLUDE_DIRS})
        target_compile_definitions(ProyecThor PRIVATE PT_HAVE_WAYLAND_CAPTURE)
    else()
        message(STATUS "libpipewire-0.3 y/o dbus-1 NO encontradas: sin captura de pantalla/ventana nativa en Wayland")
    endif()
endif()
    # Licencias de terceros y licencia principal
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/THIRD_PARTY_LICENSES.md"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/io.github.thevixcho.ProyecThor
    )

    # Iconos (tanto proyecthor.png como con App ID io.github.thevixcho.ProyecThor.png)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/proyecthor.png")
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/proyecthor.png"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
        )
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/proyecthor.png"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
            RENAME io.github.thevixcho.ProyecThor.png
        )
    endif()

    # Entrada de menu (.desktop) y Metainfo AppStream
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/io.github.thevixcho.ProyecThor.desktop")
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/io.github.thevixcho.ProyecThor.desktop"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
        )
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/proyecthor.desktop")
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/proyecthor.desktop"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
        )
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/io.github.thevixcho.ProyecThor.metainfo.xml")
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/io.github.thevixcho.ProyecThor.metainfo.xml"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo
        )
    endif()
endif()
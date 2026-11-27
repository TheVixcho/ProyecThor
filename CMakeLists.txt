cmake_minimum_required(VERSION 3.21)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project(ProyecThor
    VERSION 1.2.0
    LANGUAGES C CXX
)

# ── Detección de arquitectura y opciones de targets ───────────────────────────
set(PROYECTHOR_IS_WIN_ARM64 FALSE)
if(WIN32)
    string(TOLOWER "${CMAKE_GENERATOR_PLATFORM};${CMAKE_VS_PLATFORM_NAME};${CMAKE_SYSTEM_PROCESSOR}" _pt_win_arch)
    if(_pt_win_arch MATCHES "arm64|aarch64")
        set(PROYECTHOR_IS_WIN_ARM64 TRUE)
        message(STATUS "Arquitectura detectada: Windows ARM64")
    endif()
endif()

if(APPLE OR PROYECTHOR_IS_WIN_ARM64)
    set(_pt_default_legacy OFF)
else()
    set(_pt_default_legacy ON)
endif()
# NOTA: La interfaz clásica ImGui (target ProyecThor) fue eliminada permanentemente.
# PROYECTHOR_BUILD_LEGACY queda solo como bandera obsoleta (default OFF) por
# compatibilidad con cachés de CMake viejos; el bloque de compilación legacy ya no existe.
option(PROYECTHOR_BUILD_LEGACY "[OBSOLETO/ELIMINADO] La interfaz clásica ImGui (ProyecThor) fue retirada permanentemente" OFF)
option(PROYECTHOR_BUILD_SLINT "Compilar la nueva interfaz moderna con Slint" ON)
option(PROYECTHOR_FETCH_SLINT "Compilar Slint C++ desde fuente (FetchContent) si no hay SDK precompilado" OFF)
set(SLINT_SDK_DIR "" CACHE PATH "Ruta explícita al Slint C++ SDK (opcional)")
set(MPV_SDK_DIR "" CACHE PATH "Ruta explícita al libmpv SDK en Windows (opcional)")

# ── Acelerador de compilación con caché (ccache) para desarrollo y debug ───
# En MSVC (especialmente runners Windows con Strawberry Perl ccache) se omite por
# defecto para evitar incompatibilidades con PCH (/Yu) y PDB (/FS).
option(PROYECTHOR_USE_CCACHE "Usar ccache si esta disponible (no-MSVC)" ON)
if(PROYECTHOR_USE_CCACHE AND NOT MSVC)
    find_program(CCACHE_PROGRAM ccache)
    if(CCACHE_PROGRAM)
        message(STATUS "ccache detectado: ${CCACHE_PROGRAM} (compilación acelerada con caché habilitada)")
        set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_PROGRAM}")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
        set(ENV{CCACHE_SLOPPINESS} "pch_defines,time_macros")
        set(ENV{CCACHE_PCH_EXTERNAL_CHECKSUM} "true")
    endif()
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Tipo de compilación: Release o Debug" FORCE)
endif()

if(MSVC)
    add_compile_options(/utf-8 /EHsc /MP)
    add_compile_definitions(ssize_t=ptrdiff_t _CRT_SECURE_NO_WARNINGS _USE_MATH_DEFINES)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_definitions(PT_DEBUG)
    endif()
else()
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O2)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
        # -Og: Optimizado para compilación ultra-rápida y depuración interactiva (sin pases pesados de optimización)
        # -g1: Información de depuración esencial para stack traces y asserts rápidos sin inflar el tamaño de los .o
        # -pipe: Usa tuberías en memoria entre etapas del compilador evitando I/O de disco
        add_compile_options(-Og -g1 -pipe)
        add_compile_definitions(PT_DEBUG)
    endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(NOT DEFINED PROYECTHOR_BUILD_NUMBER)
    set(_build_count_file "${CMAKE_CURRENT_SOURCE_DIR}/build_count.txt")
    set(PROYECTHOR_BUILD_NUMBER 1)
    if(EXISTS "${_build_count_file}")
        file(READ "${_build_count_file}" PROYECTHOR_BUILD_NUMBER)
        string(STRIP "${PROYECTHOR_BUILD_NUMBER}" PROYECTHOR_BUILD_NUMBER)
    endif()
endif()

if(DEFINED PROYECTHOR_VERSION_OVERRIDE AND NOT "${PROYECTHOR_VERSION_OVERRIDE}" STREQUAL "")
    string(REPLACE "." ";" _pt_ver_list "${PROYECTHOR_VERSION_OVERRIDE}")
    list(LENGTH _pt_ver_list _pt_ver_len)
    if(_pt_ver_len GREATER_EQUAL 3)
        list(GET _pt_ver_list 0 PROJECT_VERSION_MAJOR)
        list(GET _pt_ver_list 1 PROJECT_VERSION_MINOR)
        list(GET _pt_ver_list 2 PROJECT_VERSION_PATCH)
        set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
    endif()
endif()

if(NOT DEFINED PROYECTHOR_VERSION_SUFFIX)
    set(PROYECTHOR_VERSION_SUFFIX "")
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Version.h.in"
    "${CMAKE_BINARY_DIR}/generated/Version.h"
)

find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(json)
endif()

# ── SDKs compartidos / usados por ProyecThorSlint ──────────────────────────────
# libmpv (MPV_SDK) y cpp-httplib (HTTPLIB_DIR) los usa el target Slint.
# Las rutas de IMGUI/GLFW/GLEW de la antigua interfaz ImGui fueron eliminadas.
if(MPV_SDK_DIR)
    set(MPV_SDK "${MPV_SDK_DIR}")
elseif(PROYECTHOR_IS_WIN_ARM64 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sdk/mpv-arm64")
    set(MPV_SDK "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sdk/mpv-arm64")
else()
    set(MPV_SDK "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sdk/mpv")
endif()
set(HTTPLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sdk")

if(WIN32)
    set(RESOURCE_FILE "src/resources.rc")
    if(MINGW)
        set(CMAKE_RC_FLAGS "-O coff --use-temp-file")
    endif()
else()
    set(RESOURCE_FILE "")
endif()

# ── Detección compartida de libmpv (usada por ProyecThorSlint y ProyecThor) ──
if(WIN32)
    if(MSVC)
        set(MPV_WIN_LIB "${MPV_SDK}/lib/mpv.lib")
    else()
        set(MPV_WIN_LIB "${MPV_SDK}/lib/libmpv.dll.a")
    endif()
    if(NOT EXISTS "${MPV_WIN_LIB}")
        message(FATAL_ERROR
            "No se encontro libmpv en ${MPV_WIN_LIB}\n"
            "Verifica que el SDK de libmpv este en: ${MPV_SDK}"
        )
    endif()
    message(STATUS "libmpv encontrado (Windows): ${MPV_WIN_LIB}")
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(MPV mpv)
    endif()
    if(MPV_FOUND)
        if(MPV_LINK_LIBRARIES)
            set(MPV_LINK_TARGET ${MPV_LINK_LIBRARIES})
        else()
            find_library(MPV_LIB_FALLBACK NAMES mpv libmpv PATHS ${MPV_LIBRARY_DIRS} /opt/homebrew/lib /opt/homebrew/opt/mpv/lib /usr/local/lib /usr/local/opt/mpv/lib)
            if(MPV_LIB_FALLBACK)
                set(MPV_LINK_TARGET "${MPV_LIB_FALLBACK}")
            else()
                set(MPV_LINK_TARGET ${MPV_LIBRARIES})
            endif()
        endif()
        if(NOT MPV_INCLUDE_DIRS)
            find_path(MPV_INC_FALLBACK
                NAMES mpv/client.h
                PATHS ${MPV_INCLUDEDIR} ${MPV_INCLUDE_DIRS} /opt/homebrew/include /opt/homebrew/opt/mpv/include /usr/local/include /usr/local/opt/mpv/include /usr/include
            )
            if(MPV_INC_FALLBACK)
                set(MPV_INCLUDE_DIRS "${MPV_INC_FALLBACK}")
            endif()
        endif()
        message(STATUS "libmpv encontrado via pkg-config (${MPV_VERSION}): inc=${MPV_INCLUDE_DIRS} lib=${MPV_LINK_TARGET}")
    else()
        find_library(MPV_LIB_UNIX NAMES mpv libmpv PATHS /opt/homebrew/lib /opt/homebrew/opt/mpv/lib /usr/local/lib /usr/local/opt/mpv/lib REQUIRED)
        find_path(MPV_INC_UNIX NAMES mpv/client.h PATHS /opt/homebrew/include /opt/homebrew/opt/mpv/include /usr/local/include /usr/local/opt/mpv/include REQUIRED)
        message(STATUS "libmpv encontrado (Unix/macOS): ${MPV_LIB_UNIX}")
        set(MPV_INCLUDE_DIRS "${MPV_INC_UNIX}")
        set(MPV_LINK_TARGET "${MPV_LIB_UNIX}")
    endif()
    if(APPLE)
        find_path(MPV_INC_MAC NAMES mpv/client.h PATHS ${MPV_INCLUDEDIR} ${MPV_INCLUDE_DIRS} /opt/homebrew/include /opt/homebrew/opt/mpv/include /usr/local/include /usr/local/opt/mpv/include REQUIRED)
        list(APPEND MPV_INCLUDE_DIRS "${MPV_INC_MAC}")
        list(REMOVE_DUPLICATES MPV_INCLUDE_DIRS)
        if(NOT EXISTS "${MPV_LINK_TARGET}")
            find_library(MPV_LIB_MAC NAMES mpv libmpv PATHS ${MPV_LIBRARY_DIRS} /opt/homebrew/lib /opt/homebrew/opt/mpv/lib /usr/local/lib /usr/local/opt/mpv/lib REQUIRED)
            set(MPV_LINK_TARGET "${MPV_LIB_MAC}")
        endif()
    endif()
endif()

# ══════════════════════════════════════════════════════════════════════════════
#  Interfaz clásica ImGui (target ProyecThor) — ELIMINADA PERMANENTEMENTE
#  Toda la lógica de compilación legacy (globs de fuentes, ImGui, GLFW/GLEW,
#  PDFium/WebView2, add_executable(ProyecThor ...), instalación y empaquetado)
#  fue retirada. Solo queda el target moderno ProyecThorSlint, más abajo.
# ══════════════════════════════════════════════════════════════════════════════

if(UNIX AND NOT APPLE AND NOT DEFINED PROYECTHOR_INSTALL_DIR)
    include(GNUInstallDirs)
    set(PROYECTHOR_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/proyecthor")
endif()

# ══════════════════════════════════════════════════════════════════════════════
#  ProyecThorSlint — Nueva Interfaz Moderna (Slint + libmpv, Dark Glass)
#  Soporte nativo: Linux x86_64, Windows x64 / ARM64, macOS Apple Silicon & Intel
# ══════════════════════════════════════════════════════════════════════════════
if(PROYECTHOR_BUILD_SLINT)
    set(SLINT_SDK_LINUX "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sdk/slint-linux")
    set(SLINT_SDK_MACOS "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sdk/slint-macos")
    if(PROYECTHOR_IS_WIN_ARM64 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sdk/slint-win-arm64")
        set(SLINT_SDK_WIN "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sdk/slint-win-arm64")
    else()
        set(SLINT_SDK_WIN "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sdk/slint-win")
    endif()

    set(_pt_slint_search_paths "")
    if(SLINT_SDK_DIR)
        list(APPEND _pt_slint_search_paths "${SLINT_SDK_DIR}")
    endif()
    if(APPLE)
        list(APPEND _pt_slint_search_paths "${SLINT_SDK_MACOS}")
    elseif(WIN32)
        list(APPEND _pt_slint_search_paths "${SLINT_SDK_WIN}")
    else()
        list(APPEND _pt_slint_search_paths "${SLINT_SDK_LINUX}")
    endif()

    find_package(Slint QUIET PATHS ${_pt_slint_search_paths})
    if(NOT Slint_FOUND AND PROYECTHOR_FETCH_SLINT)
        message(STATUS "Slint C++ SDK precompilado no encontrado — compilando Slint v1.19.0 desde fuente (FetchContent)...")
        if(APPLE OR PROYECTHOR_IS_WIN_ARM64)
            # Habilitar Skia garantiza Metal (CAMetalLayer) nativo en macOS y Direct3D en Windows ARM64
            set(SLINT_FEATURE_RENDERER_SKIA ON CACHE BOOL "Enable Skia renderer (Metal on macOS / D3D on Windows)" FORCE)
        endif()
        include(FetchContent)
        FetchContent_Declare(
            Slint
            GIT_REPOSITORY https://github.com/slint-ui/slint.git
            GIT_TAG        v1.19.0
            GIT_SHALLOW    TRUE
            SOURCE_SUBDIR  api/cpp
        )
        FetchContent_MakeAvailable(Slint)
        set(Slint_FOUND TRUE)
    endif()

    if(Slint_FOUND)
        message(STATUS "Slint C++ SDK encontrado (${Slint_VERSION}): habilitando target ProyecThorSlint")

        # Biblioteca estática dedicada para el código C++ autogenerado por Slint:
        # - Divide main_window.slint (~228k líneas / 22 MB) en 12 unidades paralelas (evita cuelgues de 15+ min en MSVC).
        # - Embebe los recursos SVG (@image-url) dentro del ejecutable para que funcionen en cualquier PC.
        # - Usa /Od /Ob0 /bigobj en MSVC y -O1 en GCC/Clang solo para el pegamento generado por Slint
        #   (el motor gráfico/layout ya corre en Release dentro de slint_cpp.dll / libslint_cpp.so / libslint_cpp.dylib).
        add_library(ProyecThorSlintUI STATIC)
        set_target_properties(ProyecThorSlintUI PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            SLINT_EMBED_RESOURCES "embed-files"
            # ── Traducciones (i18n) ──────────────────────────────────────
            # Las cadenas marcadas con @tr("...") en los .slint se traducen
            # con archivos gettext .po bajo translations/<lang>/LC_MESSAGES/
            # proyecthor.po. Se empaquetan dentro del binario (bundle) y se
            # eligen en runtime con slint::select_bundled_translation(...).
            # El idioma fuente (msgid) es español. Agregar un idioma nuevo =
            # crear translations/<lang>/LC_MESSAGES/proyecthor.po, sin tocar C++.
            SLINT_TRANSLATION_DOMAIN "proyecthor"
            SLINT_BUNDLE_TRANSLATIONS "${CMAKE_CURRENT_SOURCE_DIR}/translations"
        )
        slint_target_sources(ProyecThorSlintUI src/ui_slint/ui/main_window.slint COMPILATION_UNITS 12)
        target_link_libraries(ProyecThorSlintUI PUBLIC Slint::Slint)
        if(MSVC)
            target_compile_options(ProyecThorSlintUI PRIVATE /Od /Ob0 /bigobj /wd4244 /wd4267 /wd4305)
        else()
            target_compile_options(ProyecThorSlintUI PRIVATE -O1)
        endif()

        if(WIN32)
            add_executable(ProyecThorSlint WIN32
                src/ui_slint/main_slint.cpp
                src/ui_slint/SlintApp.cpp
                src/ui_slint/SlintVideoPlayer.cpp
                src/ui_slint/SlintMobileServer.cpp
                src/ui_slint/MonitorInfo.cpp
                src/core/io/BibleXmlIO.cpp
                src/core/NetworkStreamServer.cpp
                src/core/FrameEncodeWorker.cpp
                src/core/ChatMessageStore.cpp
                src/core/OSCSender.cpp
                src/core/SmartTvCaster.cpp
                src/vendor/qrcodegen/qrcodegen.cpp
                src/vendor/stb/stb_image_write_impl.cpp
                src/vendor/stb/stb_image_impl.cpp
                ${RESOURCE_FILE}
            )
        else()
            add_executable(ProyecThorSlint
                src/ui_slint/main_slint.cpp
                src/ui_slint/SlintApp.cpp
                src/ui_slint/SlintVideoPlayer.cpp
                src/ui_slint/SlintMobileServer.cpp
                src/ui_slint/MonitorInfo.cpp
                src/core/io/BibleXmlIO.cpp
                src/core/NetworkStreamServer.cpp
                src/core/FrameEncodeWorker.cpp
                src/core/ChatMessageStore.cpp
                src/core/OSCSender.cpp
                src/core/SmartTvCaster.cpp
                src/vendor/qrcodegen/qrcodegen.cpp
                src/vendor/stb/stb_image_write_impl.cpp
                src/vendor/stb/stb_image_impl.cpp
            )
        endif()

        set_target_properties(ProyecThorSlint PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
        target_compile_definitions(ProyecThorSlint PRIVATE CPPHTTPLIB_NO_EXCEPTIONS)
        if(MSVC)
            target_compile_options(ProyecThorSlint PRIVATE /bigobj)
        endif()

        target_include_directories(ProyecThorSlint PRIVATE
            "${CMAKE_BINARY_DIR}/generated"
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/core/io"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/views/biblia"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/ui_slint"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/qrcodegen"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/stb"
            "${HTTPLIB_DIR}"
        )

        target_link_libraries(ProyecThorSlint PRIVATE
            ProyecThorSlintUI
            Slint::Slint
            nlohmann_json::nlohmann_json
        )

        if(WIN32)
            target_compile_definitions(ProyecThorSlint PRIVATE
                NOMINMAX
                WIN32_LEAN_AND_MEAN
            )
            target_include_directories(ProyecThorSlint PRIVATE "${MPV_SDK}/include")
            target_link_libraries(ProyecThorSlint PRIVATE "${MPV_WIN_LIB}" shell32 ws2_32 iphlpapi)
            if(MSVC)
                target_link_options(ProyecThorSlint PRIVATE "/ENTRY:mainCRTStartup")
            endif()
            set(SLINT_WIN_DLL "")
            foreach(_d "${SLINT_SDK_DIR}" "${SLINT_SDK_WIN}")
                if(_d AND NOT SLINT_WIN_DLL)
                    if(EXISTS "${_d}/bin/slint_cpp.dll")
                        set(SLINT_WIN_DLL "${_d}/bin/slint_cpp.dll")
                    elseif(EXISTS "${_d}/lib/slint_cpp.dll")
                        set(SLINT_WIN_DLL "${_d}/lib/slint_cpp.dll")
                    endif()
                endif()
            endforeach()
            if(SLINT_WIN_DLL)
                add_custom_command(TARGET ProyecThorSlint POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${SLINT_WIN_DLL}"
                        "$<TARGET_FILE_DIR:ProyecThorSlint>"
                    COMMENT "Copiando slint_cpp.dll al directorio de salida"
                )
            endif()
            if(EXISTS "${MPV_SDK}/libmpv-2.dll")
                add_custom_command(TARGET ProyecThorSlint POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${MPV_SDK}/libmpv-2.dll"
                        "$<TARGET_FILE_DIR:ProyecThorSlint>"
                    COMMENT "Copiando libmpv-2.dll al directorio de salida"
                )
            endif()
            if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extrabuild")
                add_custom_command(TARGET ProyecThorSlint POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory
                        "${CMAKE_CURRENT_SOURCE_DIR}/extrabuild"
                        "$<TARGET_FILE_DIR:ProyecThorSlint>"
                    COMMENT "Copiando extrabuild/ al directorio de salida de ProyecThorSlint"
                )
            endif()
        elseif(APPLE)
            if(MPV_INCLUDE_DIRS)
                target_include_directories(ProyecThorSlint PRIVATE ${MPV_INCLUDE_DIRS})
            endif()
            target_link_libraries(ProyecThorSlint PRIVATE
                ${MPV_LINK_TARGET}
                pthread
                dl
            )
            set_target_properties(ProyecThorSlint PROPERTIES
                INSTALL_RPATH "@executable_path;@executable_path/../Frameworks"
                BUILD_RPATH "@executable_path;@executable_path/../Frameworks"
                BUILD_WITH_INSTALL_RPATH TRUE
                SKIP_BUILD_RPATH FALSE
            )
            set(SLINT_MACOS_DYLIB "")
            foreach(_d "${SLINT_SDK_DIR}" "${SLINT_SDK_MACOS}")
                if(_d AND NOT SLINT_MACOS_DYLIB AND EXISTS "${_d}/lib/libslint_cpp.dylib")
                    set(SLINT_MACOS_DYLIB "${_d}/lib/libslint_cpp.dylib")
                endif()
            endforeach()
            if(SLINT_MACOS_DYLIB)
                add_custom_command(TARGET ProyecThorSlint POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${SLINT_MACOS_DYLIB}"
                        "$<TARGET_FILE_DIR:ProyecThorSlint>"
                    COMMENT "Copiando libslint_cpp.dylib al directorio de salida"
                )
            endif()
        else()
            if(MPV_INCLUDE_DIRS)
                target_include_directories(ProyecThorSlint PRIVATE ${MPV_INCLUDE_DIRS})
            endif()
            target_link_libraries(ProyecThorSlint PRIVATE
                ${MPV_LINK_TARGET}
                pthread
                dl
            )
            set_target_properties(ProyecThorSlint PROPERTIES
                INSTALL_RPATH "$ORIGIN"
                BUILD_WITH_INSTALL_RPATH TRUE
                SKIP_BUILD_RPATH FALSE
            )
            if(EXISTS "${SLINT_SDK_LINUX}/lib/libslint_cpp.so")
                add_custom_command(TARGET ProyecThorSlint POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${SLINT_SDK_LINUX}/lib/libslint_cpp.so"
                        "$<TARGET_FILE_DIR:ProyecThorSlint>"
                    COMMENT "Copiando libslint_cpp.so al directorio de salida"
                )
                install(FILES "${SLINT_SDK_LINUX}/lib/libslint_cpp.so"
                    DESTINATION ${PROYECTHOR_INSTALL_DIR}
                )
            endif()
            install(TARGETS ProyecThorSlint
                RUNTIME DESTINATION ${PROYECTHOR_INSTALL_DIR}
            )
        endif()

        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
            add_custom_command(TARGET ProyecThorSlint POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
                    "$<TARGET_FILE_DIR:ProyecThorSlint>"
                COMMENT "Copiando assets/ al directorio de salida de ProyecThorSlint"
            )
        endif()
    else()
        message(STATUS "Slint C++ SDK no encontrado — omitiendo target ProyecThorSlint (usa -DPROYECTHOR_FETCH_SLINT=ON para compilar desde fuente)")
    endif()
endif()

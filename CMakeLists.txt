cmake_minimum_required(VERSION 3.11)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project(ProyecThor
    VERSION 1.1.5
    LANGUAGES C CXX
)

# ── Acelerador de compilación con caché (ccache) para desarrollo y debug ───
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "ccache detectado: ${CCACHE_PROGRAM} (compilación acelerada con caché habilitada)")
    set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(ENV{CCACHE_SLOPPINESS} "pch_defines,time_macros")
    set(ENV{CCACHE_PCH_EXTERNAL_CHECKSUM} "true")
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

find_package(glm QUIET)
if(NOT glm_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 0.9.9.8
    )
    FetchContent_MakeAvailable(glm)
endif()

add_compile_definitions(IMGUI_USE_WCHAR32 IMGUI_DEFINE_MATH_OPERATORS)

set(IMGUI_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/src/external/imgui-docking")
set(GLFW_SDK   "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sdk/glfw")
set(GLEW_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sdk/glew")
set(MPV_SDK    "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sdk/mpv")
set(PDFIUM_SDK "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sdk/pdfium")
set(PDFIUM_SDK_LINUX "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sdk/pdfium-linux")
set(WEBVIEW2_SDK "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sdk/webview2")
set(HTTPLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sdk")

file(GLOB CORE_SOURCES              CONFIGURE_DEPENDS "src/core/*.cpp")
file(GLOB CORE_PLUGINS_SOURCES      CONFIGURE_DEPENDS "src/core/plugins/*.cpp")
file(GLOB CORE_IO_SOURCES           CONFIGURE_DEPENDS "src/core/io/*.cpp")
file(GLOB CORE_SETTINGS_SOURCES     CONFIGURE_DEPENDS "src/core/settings/*.cpp")

file(GLOB MEDIA_PLAYER_SOURCES      CONFIGURE_DEPENDS "src/media/player/*.cpp")
file(GLOB MEDIA_AUDIO_SOURCES       CONFIGURE_DEPENDS "src/media/audio/*.cpp")
file(GLOB MEDIA_CAPTURE_SOURCES     CONFIGURE_DEPENDS "src/media/capture/*.cpp")

file(GLOB GRAPHICS_SHADERS_SOURCES  CONFIGURE_DEPENDS "src/graphics/shaders/*.cpp")
file(GLOB EXTERNAL_TOOLS            CONFIGURE_DEPENDS "src/external/tools/*.cpp")

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

set(VENDOR_SOURCES
    "src/vendor/stb/stb_image_write_impl.cpp"
    "src/vendor/qrcodegen/qrcodegen.cpp"
)

file(GLOB ROOT_SOURCES              CONFIGURE_DEPENDS "src/*.cpp")

set(IMGUI_SOURCES
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)

if(WIN32)
    set(RESOURCE_FILE "src/resources.rc")
    if(MINGW)
        set(CMAKE_RC_FLAGS "-O coff --use-temp-file")
    endif()
else()
    set(RESOURCE_FILE "")
endif()

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/shaders"
     DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/plugins")
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/plugins"
         DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
endif()

if(WIN32 AND EXISTS "${PDFIUM_SDK}/bin/pdfium.dll")
    file(COPY "${PDFIUM_SDK}/bin/pdfium.dll"
         DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
endif()

if(WIN32)
    add_executable(ProyecThor WIN32
        ${ROOT_SOURCES}
        ${CORE_SOURCES}
        ${CORE_PLUGINS_SOURCES}
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
        ${CORE_PLUGINS_SOURCES}
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

target_compile_definitions(ProyecThor PRIVATE
    GLEW_STATIC
    GLEW_NO_GLU
    GLFW_INCLUDE_NONE
    CPPHTTPLIB_NO_EXCEPTIONS
)

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
    "${CMAKE_BINARY_DIR}/generated"

    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/settings"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/io"

    "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/shaders"

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

    "${CMAKE_CURRENT_SOURCE_DIR}/src/media"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/media/player"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/media/audio"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/media/capture"

    "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/stb"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/nanosvg"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/qrcodegen"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sdk"

    "${CMAKE_CURRENT_SOURCE_DIR}/src/external/tools"
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
    "${GLFW_SDK}/include"
    "${GLEW_DIR}/include"

    "${HTTPLIB_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)

if(WIN32)
    target_include_directories(ProyecThor PRIVATE
        "${PDFIUM_SDK}/include"
        "${WEBVIEW2_SDK}/include"
        "${MPV_SDK}/include"
    )
else()
    target_include_directories(ProyecThor PRIVATE
        "${PDFIUM_SDK_LINUX}/include"
    )
endif()

target_link_libraries(ProyecThor PRIVATE
    nlohmann_json::nlohmann_json
    glm::glm
)

# ── Precompiled Headers (PCH) para acelerar drásticamente la compilación ───────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h")
    target_precompile_headers(ProyecThor PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h>"
    )
    message(STATUS "Precompiled Headers (PCH) habilitados para C++: ${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h")
endif()

if(WIN32)
    set(PDFIUM_LIB "${PDFIUM_SDK}/lib/pdfium.dll.lib")

    if(NOT EXISTS "${PDFIUM_LIB}")
        message(FATAL_ERROR
            "No se encontro pdfium.dll.lib en ${PDFIUM_SDK}/lib\n"
            "Verifica que el archivo existe en: ${PDFIUM_LIB}"
        )
    endif()

    message(STATUS "PDFium encontrado (Windows): ${PDFIUM_LIB}")

    if(MSVC)
        set(GLFW_WIN_LIB "${GLFW_SDK}/lib-vc2022/glfw3.lib")
        set(MPV_WIN_LIB  "${MPV_SDK}/lib/mpv.lib")
    else()
        set(GLFW_WIN_LIB "${GLFW_SDK}/lib-mingw-w64/libglfw3.a")
        set(MPV_WIN_LIB  "${MPV_SDK}/lib/libmpv.dll.a")
    endif()

    if(NOT EXISTS "${MPV_WIN_LIB}")
        message(FATAL_ERROR
            "No se encontro libmpv en ${MPV_WIN_LIB}\n"
            "Verifica que el SDK de libmpv este en: ${MPV_SDK}"
        )
    endif()
    message(STATUS "libmpv encontrado (Windows): ${MPV_WIN_LIB}")

    target_link_libraries(ProyecThor PRIVATE
        "${GLFW_WIN_LIB}"
        "${MPV_WIN_LIB}"
        "${PDFIUM_LIB}"
        "${WEBVIEW2_SDK}/x64/WebView2Loader.dll.lib"
        opengl32
        user32
        ole32
        gdi32
        shell32
        windowsapp
        dwmapi
        comdlg32
        winmm
        ws2_32
        iphlpapi
        winhttp
        d3d11
        dxgi
        dxguid
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

    if(MSVC)
        target_link_options(ProyecThor PRIVATE
            "/ENTRY:mainCRTStartup"
        )
    elseif(MINGW)
        target_link_options(ProyecThor PRIVATE
            "-static"
        )
    endif()

else()

    find_package(glfw3 3.4 QUIET)
    if(NOT glfw3_FOUND)
        find_package(glfw3 QUIET)
    endif()
    if(NOT glfw3_FOUND)
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
    endif()
    if(TARGET glfw3::glfw AND NOT TARGET glfw)
        add_library(glfw ALIAS glfw3::glfw)
    endif()

    find_package(OpenGL REQUIRED)

    find_package(OpenCV QUIET COMPONENTS core videoio)
    if(OpenCV_FOUND)
        message(STATUS "OpenCV encontrado (${OpenCV_VERSION}): deteccion real de camaras habilitada en Captura")
        target_link_libraries(ProyecThor PRIVATE ${OpenCV_LIBS})
        target_include_directories(ProyecThor PRIVATE ${OpenCV_INCLUDE_DIRS})
        target_compile_definitions(ProyecThor PRIVATE PT_USE_OPENCV)
    else()
        message(STATUS "OpenCV NO encontrado: Captura seguira usando la camara simulada de prueba")
    endif()

    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(MPV mpv)
    endif()
    if(MPV_FOUND)
        message(STATUS "libmpv encontrado via pkg-config (${MPV_VERSION}): ${MPV_LIBRARIES}")
        target_include_directories(ProyecThor PRIVATE ${MPV_INCLUDE_DIRS})
        set(MPV_LINK_TARGET ${MPV_LIBRARIES})
    else()
        find_library(MPV_LIB_LINUX NAMES mpv libmpv REQUIRED)
        find_path(MPV_INC_LINUX NAMES mpv/client.h REQUIRED)
        message(STATUS "libmpv encontrado (Linux): ${MPV_LIB_LINUX}")
        target_include_directories(ProyecThor PRIVATE "${MPV_INC_LINUX}")
        set(MPV_LINK_TARGET "${MPV_LIB_LINUX}")
    endif()

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
        ${MPV_LINK_TARGET}
        "${PDFIUM_LIB_LINUX}"
        pthread
        dl
    )

    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(GIO gio-2.0)
        if(GIO_FOUND)
            message(STATUS "GIO (2.0) encontrado: Portal FileChooser nativo habilitado para Flatpak y Linux")
            target_link_libraries(ProyecThor PRIVATE ${GIO_LIBRARIES})
            target_include_directories(ProyecThor PRIVATE ${GIO_INCLUDE_DIRS})
            target_compile_definitions(ProyecThor PRIVATE PT_HAVE_GIO_PORTAL)
        endif()
    endif()

    if(PROYECTHOR_HAS_OPENSSL)
        target_link_libraries(ProyecThor PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    endif()
endif()

if(WIN32)
    if(EXISTS "${PDFIUM_SDK}/bin/pdfium.dll")
        add_custom_command(TARGET ProyecThor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PDFIUM_SDK}/bin/pdfium.dll"
                "$<TARGET_FILE_DIR:ProyecThor>"
            COMMENT "Copiando pdfium.dll al directorio de salida"
        )
    endif()

    if(EXISTS "${MPV_SDK}/libmpv-2.dll")
        add_custom_command(TARGET ProyecThor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MPV_SDK}/libmpv-2.dll"
                "$<TARGET_FILE_DIR:ProyecThor>"
            COMMENT "Copiando libmpv-2.dll al directorio de salida"
        )
    endif()

    if(EXISTS "${WEBVIEW2_SDK}/x64/WebView2Loader.dll")
        add_custom_command(TARGET ProyecThor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${WEBVIEW2_SDK}/x64/WebView2Loader.dll"
                "$<TARGET_FILE_DIR:ProyecThor>"
            COMMENT "Copiando WebView2Loader.dll al directorio de salida"
        )
    endif()
endif()

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

if(NOT WIN32)
    add_custom_command(TARGET ProyecThor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PDFIUM_SDK_LINUX}/lib/libpdfium.so"
            "$<TARGET_FILE_DIR:ProyecThor>"
        COMMENT "Copiando libpdfium.so al directorio de build"
    )
endif()

if(NOT WIN32)
    include(GNUInstallDirs)

    set(PROYECTHOR_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/proyecthor")

    set_target_properties(ProyecThor PROPERTIES
        INSTALL_RPATH "$ORIGIN"
        BUILD_WITH_INSTALL_RPATH TRUE
        SKIP_BUILD_RPATH FALSE
    )

    install(TARGETS ProyecThor
        RUNTIME DESTINATION ${PROYECTHOR_INSTALL_DIR}
    )

    install(FILES "${PDFIUM_SDK_LINUX}/lib/libpdfium.so"
        DESTINATION ${PROYECTHOR_INSTALL_DIR}
    )

    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders"
        DESTINATION ${PROYECTHOR_INSTALL_DIR}
    )

    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/proyecthor-wrapper.sh"
"#!/bin/sh
SCRIPT_DIR=\"\$(dirname \"\$(readlink -f \"\$0\")\")\"
APP_DIR=\"\$(cd \"\$SCRIPT_DIR/..\" && pwd)\"
if [ -d \"\$APP_DIR/lib/proyecthor\" ]; then
    INSTALL_DIR=\"\$APP_DIR/lib/proyecthor\"
elif [ -d \"\$APP_DIR/lib64/proyecthor\" ]; then
    INSTALL_DIR=\"\$APP_DIR/lib64/proyecthor\"
else
    INSTALL_DIR=\"${CMAKE_INSTALL_PREFIX}/${PROYECTHOR_INSTALL_DIR}\"
fi
cd \"\$INSTALL_DIR\" || exit 1
if [ -x ./ProyecThorSlint ]; then
    exec ./ProyecThorSlint \"\$@\"
fi
exec ./ProyecThor \"\$@\"
")
    install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/proyecthor-wrapper.sh"
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        RENAME proyecthor
    )

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
        install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/"
            DESTINATION ${PROYECTHOR_INSTALL_DIR}
        )
    else()
        message(WARNING "assets/ NO encontrado, el paquete no tendra iconos/fuente/splash")
    endif()

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

    if(X11_Xcomposite_FOUND)
        message(STATUS "libXcomposite encontrada: captura de ventanas correcta bajo solapamiento")
        target_link_libraries(ProyecThor PRIVATE ${X11_Xcomposite_LIB})
        target_include_directories(ProyecThor PRIVATE ${X11_Xcomposite_INCLUDE_PATH})
        target_compile_definitions(ProyecThor PRIVATE PT_HAVE_XCOMPOSITE)
    else()
        message(STATUS "libXcomposite NO encontrada: captura de ventanas usara XGetImage directo (puede fallar si hay solapamiento)")
    endif()

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
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/THIRD_PARTY_LICENSES.md"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/io.github.thevixcho.ProyecThor
    )

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/proyecthor.png")
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/proyecthor.png"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
        )
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/proyecthor.png"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
            RENAME io.github.thevixcho.ProyecThor.png
        )
    endif()

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

# ══════════════════════════════════════════════════════════════════════════════
#  ProyecThorSlint — Nueva Interfaz Moderna (Slint + libmpv, Dark Glass)
#  Migración incremental en rama beta manteniendo compatibilidad Linux + Windows
# ══════════════════════════════════════════════════════════════════════════════
option(PROYECTHOR_BUILD_SLINT "Compilar la nueva interfaz moderna con Slint" ON)
if(PROYECTHOR_BUILD_SLINT)
    set(SLINT_SDK_LINUX "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sdk/slint-linux")
    set(SLINT_SDK_WIN   "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sdk/slint-win")

    find_package(Slint QUIET PATHS "${SLINT_SDK_LINUX}" "${SLINT_SDK_WIN}")
    if(Slint_FOUND)
        message(STATUS "Slint C++ SDK encontrado (${Slint_VERSION}): habilitando target ProyecThorSlint")

        if(WIN32)
            add_executable(ProyecThorSlint WIN32
                src/ui_slint/main_slint.cpp
                src/ui_slint/SlintApp.cpp
                src/ui_slint/SlintVideoPlayer.cpp
                src/ui_slint/SlintMobileServer.cpp
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

        slint_target_sources(ProyecThorSlint src/ui_slint/ui/main_window.slint)

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
            Slint::Slint
            nlohmann_json::nlohmann_json
        )

        if(WIN32)
            target_compile_definitions(ProyecThorSlint PRIVATE
                NOMINMAX
                WIN32_LEAN_AND_MEAN
            )
            target_include_directories(ProyecThorSlint PRIVATE "${MPV_SDK}/include")
            target_link_libraries(ProyecThorSlint PRIVATE "${MPV_WIN_LIB}" ws2_32 iphlpapi)
            if(MSVC)
                target_link_options(ProyecThorSlint PRIVATE "/ENTRY:mainCRTStartup")
            endif()
            set(SLINT_WIN_DLL "")
            if(EXISTS "${SLINT_SDK_WIN}/bin/slint_cpp.dll")
                set(SLINT_WIN_DLL "${SLINT_SDK_WIN}/bin/slint_cpp.dll")
            elseif(EXISTS "${SLINT_SDK_WIN}/lib/slint_cpp.dll")
                set(SLINT_WIN_DLL "${SLINT_SDK_WIN}/lib/slint_cpp.dll")
            endif()
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
        else()
            if(MPV_FOUND)
                target_include_directories(ProyecThorSlint PRIVATE ${MPV_INCLUDE_DIRS})
            elseif(MPV_INC_LINUX)
                target_include_directories(ProyecThorSlint PRIVATE "${MPV_INC_LINUX}")
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
        message(STATUS "Slint C++ SDK no encontrado en src/vendor/sdk/slint-{linux,win} — omitiendo target ProyecThorSlint")
    endif()
endif()

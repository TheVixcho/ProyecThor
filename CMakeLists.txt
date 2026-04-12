cmake_minimum_required(VERSION 3.11) # Recomiendo 3.11+ para FetchContent

project(ProyecThor C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# ── 1. DEPENDENCIAS EXTERNAS (JSON) ──────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3 # Versión estable
)
FetchContent_MakeAvailable(json)

# ── 2. RUTAS DE LIBRERÍAS ────────────────────────────────────────────────────
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/external/imgui-docking")
set(GLFW_SDK   "${CMAKE_CURRENT_SOURCE_DIR}/src/sdk/glfw")
set(GLEW_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/src/sdk/glew")
set(VLC_SDK    "${CMAKE_CURRENT_SOURCE_DIR}/src/sdk/vlc")

# ── 3. ARCHIVOS FUENTE (Siguiendo tu estilo GLOB) ────────────────────────────
file(GLOB SOURCES CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB CORE_SOURCES CONFIGURE_DEPENDS "src/core/*.cpp")
file(GLOB UI_SOURCES CONFIGURE_DEPENDS "src/ui/*.cpp")
file(GLOB PANEL_SOURCES CONFIGURE_DEPENDS "src/ui/panels/*.cpp")
file(GLOB PREVIEW_SOURCES CONFIGURE_DEPENDS "src/ui/panels/preview/*.cpp")
# Detecta automáticamente todos los .cpp de las categorías (Theme, Audio, etc.)
file(GLOB SETTINGS_SOURCES CONFIGURE_DEPENDS "src/ui/settings/*.cpp") 
file(GLOB EXTERNAL_TOOLS CONFIGURE_DEPENDS "src/external/tools/*.cpp")

set(IMGUI_SOURCES
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)

# ── 4. CREACIÓN DEL EJECUTABLE ───────────────────────────────────────────────
add_executable(ProyecThor 
    ${SOURCES} 
    ${CORE_SOURCES}
    ${UI_SOURCES} 
    ${PANEL_SOURCES} 
    ${PREVIEW_SOURCES}
    ${SETTINGS_SOURCES}
    ${IMGUI_SOURCES}
    ${EXTERNAL_TOOLS}
    "${GLEW_DIR}/src/glew.c"
)

# ── 5. DEFINICIONES Y DIRECTORIOS ────────────────────────────────────────────
target_compile_definitions(ProyecThor PRIVATE GLEW_STATIC GLFW_INCLUDE_NONE)

target_include_directories(ProyecThor PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/panels"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/settings"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/external/tools"
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
    "${GLFW_SDK}/include"
    "${GLEW_DIR}/include"
    "${VLC_SDK}/include"
)

# ── 6. LINKEO DE LIBRERÍAS ───────────────────────────────────────────────────
target_link_libraries(ProyecThor PRIVATE 
    nlohmann_json::nlohmann_json  # La librería JSON que bajamos arriba
    "${GLFW_SDK}/lib-mingw-w64/libglfw3.a"
    "${VLC_SDK}/lib/libvlc.lib"      
    "${VLC_SDK}/lib/libvlccore.lib"  
    opengl32
    gdi32
    shell32
)

# WinHTTP para el sistema de actualizaciones (CategoryUpdates.cpp)
if(WIN32)
    target_link_libraries(ProyecThor PRIVATE winhttp)
endif()

# ── 7. OPCIONES DE COMPILACIÓN (Estático) ────────────────────────────────────
target_link_options(ProyecThor PRIVATE 
    -static-libgcc 
    -static-libstdc++ 
    -static
)
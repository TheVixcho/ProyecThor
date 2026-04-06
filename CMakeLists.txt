cmake_minimum_required(VERSION 3.10)

# Agregamos C para que CMake sepa manejar el archivo glew.c y libVLC
project(ProyecThor C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# --- RUTAS DE LIBRERÍAS ---
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/external/imgui-docking")
set(GLFW_SDK  "${CMAKE_CURRENT_SOURCE_DIR}/src/sdk/glfw")
set(GLEW_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/src/sdk/glew")
set(VLC_SDK   "${CMAKE_CURRENT_SOURCE_DIR}/src/sdk/vlc") # Tu nueva ruta

# --- ARCHIVOS FUENTE ---
file(GLOB SOURCES "src/*.cpp")
file(GLOB CORE_SOURCES "src/core/*.cpp")
file(GLOB UI_SOURCES "src/ui/*.cpp")
file(GLOB PANEL_SOURCES "src/ui/panels/*.cpp")
file(GLOB EXTERNAL_TOOLS "src/external/tools/*.cpp")

set(IMGUI_SOURCES
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)

# --- CREACIÓN DEL EJECUTABLE ---
add_executable(ProyecThor 
    ${SOURCES} 
    ${CORE_SOURCES}
    ${UI_SOURCES} 
    ${PANEL_SOURCES} 
    ${IMGUI_SOURCES}
    ${EXTERNAL_TOOLS}
    "${GLEW_DIR}/src/glew.c"
)

# --- DEFINICIONES ---
target_compile_definitions(ProyecThor PRIVATE 
    GLEW_STATIC 
    GLFW_INCLUDE_NONE
)

# --- INCLUDES (Donde el compilador busca los .h) ---
target_include_directories(ProyecThor PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/panels"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/external/tools"
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
    "${GLFW_SDK}/include"
    "${GLEW_DIR}/include"
    "${VLC_SDK}/include" # Cabeceras de VLC
)

# --- LINKEO (Donde se conectan los .a / .lib) ---
target_link_libraries(ProyecThor PRIVATE 
    "${GLFW_SDK}/lib-mingw-w64/libglfw3.a"
    "${VLC_SDK}/lib/libvlc.lib"      # Archivo de linkeo de VLC
    "${VLC_SDK}/lib/libvlccore.lib"  # Archivo de linkeo de VLC Core
    opengl32
    gdi32
    shell32
)

# --- OPCIONES DE MIN-GW ---
# Se mantiene la configuración estática para evitar dependencias externas de GCC
target_link_options(ProyecThor PRIVATE 
    -static-libgcc 
    -static-libstdc++ 
    -static
)
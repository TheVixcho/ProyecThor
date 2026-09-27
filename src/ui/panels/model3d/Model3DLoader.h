#pragma once
#include "Model3DTypes.h"
#include <string>
#include <vector>

namespace ProyecThor::UI {

class Model3DLoader {
public:
    // Carga un archivo 3D (OBJ, STL, PLY) y llena la estructura de malla
    static bool LoadModel(const std::string& filePath, Model3DMesh& outMesh, Model3DFormat& outFormat);

    // Parsers individuales
    static bool LoadOBJ(const std::string& filePath, Model3DMesh& outMesh);
    static bool LoadSTL(const std::string& filePath, Model3DMesh& outMesh);
    static bool LoadPLY(const std::string& filePath, Model3DMesh& outMesh);
    static bool LoadGLTF(const std::string& filePath, Model3DMesh& outMesh);
    static bool LoadGLB(const std::string& filePath, Model3DMesh& outMesh);

    // Creador de primitivas y modelos 3D integrados
    static Model3DMesh CreatePrimitive(const std::string& primitiveName);
    static std::vector<std::string> GetAvailablePrimitives();

    // Escanea un directorio en busca de archivos 3D soportados (.obj, .stl, .ply)
    static std::vector<Model3DAsset> ScanDirectory(const std::string& folderPath);

    // Calcula normales por vértice si no existen y normaliza la caja delimitadora a [-1, 1]
    static void ComputeNormalsAndBounds(Model3DMesh& mesh);
};

} // namespace ProyecThor::UI


#include "Model3DLoader.h"
#include <GL/glew.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstring>
#include <cstdint>
#include <map>
#include <tuple>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace ProyecThor::UI {

// ── Model3DMesh GPU Resource Management ─────────────────────────────────────

void Model3DMesh::UploadToGpu() {
    ReleaseGpu();
    if (vertices.empty() || indices.empty()) return;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Model3DVertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

    // Layout: 0 = Position (Vec3), 1 = Normal (Vec3), 2 = UV (Vec2)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Model3DVertex), (void*)offsetof(Model3DVertex, position));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Model3DVertex), (void*)offsetof(Model3DVertex, normal));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Model3DVertex), (void*)offsetof(Model3DVertex, uv));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    isGpuLoaded = true;
}

void Model3DMesh::ReleaseGpu() {
    if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
    if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
    if (ebo) { glDeleteBuffers(1, &ebo); ebo = 0; }
    isGpuLoaded = false;
}

// ── Bounds and Normals Computation ──────────────────────────────────────────

void Model3DLoader::ComputeNormalsAndBounds(Model3DMesh& mesh) {
    if (mesh.vertices.empty()) return;

    // 1. Calcular caja envolvente y centro
    Vec3 bMin = mesh.vertices[0].position;
    Vec3 bMax = mesh.vertices[0].position;

    for (const auto& v : mesh.vertices) {
        bMin.x = std::min(bMin.x, v.position.x);
        bMin.y = std::min(bMin.y, v.position.y);
        bMin.z = std::min(bMin.z, v.position.z);

        bMax.x = std::max(bMax.x, v.position.x);
        bMax.y = std::max(bMax.y, v.position.y);
        bMax.z = std::max(bMax.z, v.position.z);
    }

    Vec3 center = (bMin + bMax) * 0.5f;
    float maxDist = 0.001f;

    for (const auto& v : mesh.vertices) {
        float d = (v.position - center).Length();
        if (d > maxDist) maxDist = d;
    }

    // Normalizar a radio unitario y centrar
    float scaleFactor = 1.0f / maxDist;
    for (auto& v : mesh.vertices) {
        v.position = (v.position - center) * scaleFactor;
    }

    mesh.bboxMin = (bMin - center) * scaleFactor;
    mesh.bboxMax = (bMax - center) * scaleFactor;
    mesh.center  = { 0, 0, 0 };
    mesh.radius  = 1.0f;

    // 2. Comprobar si las normales son válidas; si no, calcular normales de faceta
    bool needNormals = false;
    for (const auto& v : mesh.vertices) {
        if (v.normal.LengthSq() < 0.01f) {
            needNormals = true;
            break;
        }
    }

    if (needNormals && !mesh.indices.empty()) {
        for (auto& v : mesh.vertices) v.normal = { 0, 0, 0 };

        for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            uint32_t i0 = mesh.indices[i];
            uint32_t i1 = mesh.indices[i + 1];
            uint32_t i2 = mesh.indices[i + 2];

            if (i0 < mesh.vertices.size() && i1 < mesh.vertices.size() && i2 < mesh.vertices.size()) {
                Vec3 p0 = mesh.vertices[i0].position;
                Vec3 p1 = mesh.vertices[i1].position;
                Vec3 p2 = mesh.vertices[i2].position;

                Vec3 fn = Vec3::Cross(p1 - p0, p2 - p0);
                mesh.vertices[i0].normal += fn;
                mesh.vertices[i1].normal += fn;
                mesh.vertices[i2].normal += fn;
            }
        }

        for (auto& v : mesh.vertices) {
            v.normal = v.normal.Normalized();
        }
    }
}

// ── OBJ Parser ──────────────────────────────────────────────────────────────

bool Model3DLoader::LoadOBJ(const std::string& filePath, Model3DMesh& outMesh) {
    std::ifstream file(filePath);
    if (!file.is_open()) return false;

    outMesh.vertices.clear();
    outMesh.indices.clear();
    outMesh.name = fs::path(filePath).stem().string();

    std::vector<Vec3> tempPositions;
    std::vector<Vec3> tempNormals;
    std::vector<Vec2> tempUVs;

    std::map<std::tuple<int, int, int>, uint32_t> vertexMap;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        std::string prefix;
        ss >> prefix;

        if (prefix == "v") {
            Vec3 pos;
            ss >> pos.x >> pos.y >> pos.z;
            tempPositions.push_back(pos);
        } else if (prefix == "vn") {
            Vec3 norm;
            ss >> norm.x >> norm.y >> norm.z;
            tempNormals.push_back(norm);
        } else if (prefix == "vt") {
            Vec2 uv;
            ss >> uv.x >> uv.y;
            tempUVs.push_back(uv);
        } else if (prefix == "f") {
            std::vector<uint32_t> faceIndices;
            std::string vertToken;

            auto SafeInt = [](const std::string& str) -> int {
                if (str.empty()) return 0;
                try {
                    return std::stoi(str);
                } catch (...) {
                    return 0;
                }
            };

            while (ss >> vertToken) {
                int pIdx = 0, uvIdx = 0, nIdx = 0;
                // Parse v, v/vt, v//vn, v/vt/vn
                size_t s1 = vertToken.find('/');
                if (s1 == std::string::npos) {
                    pIdx = SafeInt(vertToken);
                } else {
                    pIdx = SafeInt(vertToken.substr(0, s1));
                    size_t s2 = vertToken.find('/', s1 + 1);
                    if (s2 == std::string::npos) {
                        std::string t = vertToken.substr(s1 + 1);
                        if (!t.empty()) uvIdx = SafeInt(t);
                    } else {
                        std::string t1 = vertToken.substr(s1 + 1, s2 - s1 - 1);
                        std::string t2 = vertToken.substr(s2 + 1);
                        if (!t1.empty()) uvIdx = SafeInt(t1);
                        if (!t2.empty()) nIdx = SafeInt(t2);
                    }
                }

                // Handle 1-based and negative indices
                if (pIdx < 0) pIdx = (int)tempPositions.size() + pIdx + 1;
                if (uvIdx < 0) uvIdx = (int)tempUVs.size() + uvIdx + 1;
                if (nIdx < 0) nIdx = (int)tempNormals.size() + nIdx + 1;

                auto key = std::make_tuple(pIdx, uvIdx, nIdx);
                auto it = vertexMap.find(key);
                uint32_t finalIdx = 0;

                if (it != vertexMap.end()) {
                    finalIdx = it->second;
                } else {
                    Model3DVertex vert;
                    if (pIdx > 0 && pIdx <= (int)tempPositions.size())
                        vert.position = tempPositions[pIdx - 1];
                    if (nIdx > 0 && nIdx <= (int)tempNormals.size())
                        vert.normal = tempNormals[nIdx - 1];
                    if (uvIdx > 0 && uvIdx <= (int)tempUVs.size())
                        vert.uv = tempUVs[uvIdx - 1];

                    finalIdx = (uint32_t)outMesh.vertices.size();
                    outMesh.vertices.push_back(vert);
                    vertexMap[key] = finalIdx;
                }
                faceIndices.push_back(finalIdx);
            }

            // Triangulate convex polygons (fan triangulation)
            for (size_t i = 1; i + 1 < faceIndices.size(); ++i) {
                outMesh.indices.push_back(faceIndices[0]);
                outMesh.indices.push_back(faceIndices[i]);
                outMesh.indices.push_back(faceIndices[i + 1]);
            }
        }
    }

    if (outMesh.vertices.empty() || outMesh.indices.empty()) return false;

    ComputeNormalsAndBounds(outMesh);
    return true;
}

// ── STL Parser (Binary and ASCII) ───────────────────────────────────────────

bool Model3DLoader::LoadSTL(const std::string& filePath, Model3DMesh& outMesh) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;

    outMesh.vertices.clear();
    outMesh.indices.clear();
    outMesh.name = fs::path(filePath).stem().string();

    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    if (fileSize < 84) return false;

    char header[80];
    file.read(header, 80);

    uint32_t triangleCount = 0;
    file.read(reinterpret_cast<char*>(&triangleCount), 4);

    bool isBinary = (fileSize == 84 + (size_t)triangleCount * 50);

    if (isBinary && triangleCount > 0) {
        outMesh.vertices.reserve(triangleCount * 3);
        outMesh.indices.reserve(triangleCount * 3);

        for (uint32_t i = 0; i < triangleCount; ++i) {
            float norm[3], v0[3], v1[3], v2[3];
            uint16_t attr = 0;

            file.read(reinterpret_cast<char*>(norm), 12);
            file.read(reinterpret_cast<char*>(v0), 12);
            file.read(reinterpret_cast<char*>(v1), 12);
            file.read(reinterpret_cast<char*>(v2), 12);
            file.read(reinterpret_cast<char*>(&attr), 2);

            Vec3 fn = { norm[0], norm[1], norm[2] };
            if (fn.LengthSq() < 0.01f) {
                Vec3 p0 = { v0[0], v0[1], v0[2] };
                Vec3 p1 = { v1[0], v1[1], v1[2] };
                Vec3 p2 = { v2[0], v2[1], v2[2] };
                fn = Vec3::Cross(p1 - p0, p2 - p0).Normalized();
            }

            uint32_t baseIdx = (uint32_t)outMesh.vertices.size();
            outMesh.vertices.push_back({ { v0[0], v0[1], v0[2] }, fn, { 0, 0 } });
            outMesh.vertices.push_back({ { v1[0], v1[1], v1[2] }, fn, { 0, 0 } });
            outMesh.vertices.push_back({ { v2[0], v2[1], v2[2] }, fn, { 0, 0 } });

            outMesh.indices.push_back(baseIdx);
            outMesh.indices.push_back(baseIdx + 1);
            outMesh.indices.push_back(baseIdx + 2);
        }
    } else {
        // ASCII STL
        file.seekg(0, std::ios::beg);
        std::string word;
        Vec3 currentNormal = { 0, 1, 0 };
        std::vector<Vec3> triVerts;

        while (file >> word) {
            if (word == "facet") {
                std::string normalWord;
                file >> normalWord; // "normal"
                file >> currentNormal.x >> currentNormal.y >> currentNormal.z;
                triVerts.clear();
            } else if (word == "vertex") {
                Vec3 v;
                file >> v.x >> v.y >> v.z;
                triVerts.push_back(v);
            } else if (word == "endfacet") {
                if (triVerts.size() >= 3) {
                    uint32_t baseIdx = (uint32_t)outMesh.vertices.size();
                    outMesh.vertices.push_back({ triVerts[0], currentNormal, { 0, 0 } });
                    outMesh.vertices.push_back({ triVerts[1], currentNormal, { 0, 0 } });
                    outMesh.vertices.push_back({ triVerts[2], currentNormal, { 0, 0 } });

                    outMesh.indices.push_back(baseIdx);
                    outMesh.indices.push_back(baseIdx + 1);
                    outMesh.indices.push_back(baseIdx + 2);
                }
            }
        }
    }

    if (outMesh.vertices.empty() || outMesh.indices.empty()) return false;

    ComputeNormalsAndBounds(outMesh);
    return true;
}

// ── PLY Parser ──────────────────────────────────────────────────────────────

bool Model3DLoader::LoadPLY(const std::string& filePath, Model3DMesh& outMesh) {
    std::ifstream file(filePath);
    if (!file.is_open()) return false;

    outMesh.vertices.clear();
    outMesh.indices.clear();
    outMesh.name = fs::path(filePath).stem().string();

    std::string line;
    int numVertices = 0;
    int numFaces = 0;
    bool inHeader = true;

    while (inHeader && std::getline(file, line)) {
        std::stringstream ss(line);
        std::string tag;
        ss >> tag;
        if (tag == "element") {
            std::string elemType;
            int count = 0;
            ss >> elemType >> count;
            if (elemType == "vertex") numVertices = count;
            else if (elemType == "face") numFaces = count;
        } else if (tag == "end_header") {
            inHeader = false;
        }
    }

    if (numVertices <= 0) return false;

    outMesh.vertices.reserve(numVertices);
    for (int i = 0; i < numVertices; ++i) {
        if (!std::getline(file, line)) break;
        std::stringstream ss(line);
        Model3DVertex v;
        ss >> v.position.x >> v.position.y >> v.position.z;
        // Optionally read normals if present in line
        if (!(ss >> v.normal.x >> v.normal.y >> v.normal.z)) {
            v.normal = { 0, 1, 0 };
        }
        outMesh.vertices.push_back(v);
    }

    for (int i = 0; i < numFaces; ++i) {
        if (!std::getline(file, line)) break;
        std::stringstream ss(line);
        int count = 0;
        ss >> count;
        std::vector<uint32_t> f;
        for (int c = 0; c < count; ++c) {
            uint32_t idx;
            ss >> idx;
            f.push_back(idx);
        }
        for (size_t k = 1; k + 1 < f.size(); ++k) {
            outMesh.indices.push_back(f[0]);
            outMesh.indices.push_back(f[k]);
            outMesh.indices.push_back(f[k + 1]);
        }
    }

    if (outMesh.vertices.empty() || outMesh.indices.empty()) return false;

    ComputeNormalsAndBounds(outMesh);
    return true;
}

// ── Procedural Built-in 3D Primitives ───────────────────────────────────────

static void AddBoxToMesh(Model3DMesh& mesh, Vec3 min, Vec3 max) {
    Vec3 corners[8] = {
        { min.x, min.y, min.z }, // 0
        { max.x, min.y, min.z }, // 1
        { max.x, max.y, min.z }, // 2
        { min.x, max.y, min.z }, // 3
        { min.x, min.y, max.z }, // 4
        { max.x, min.y, max.z }, // 5
        { max.x, max.y, max.z }, // 6
        { min.x, max.y, max.z }  // 7
    };

    struct FaceDef { int i0, i1, i2, i3; Vec3 n; };
    FaceDef faces[6] = {
        { 0, 3, 2, 1, { 0, 0, -1 } }, // Front (-Z)
        { 5, 6, 7, 4, { 0, 0,  1 } }, // Back (+Z)
        { 4, 7, 3, 0, {-1, 0,  0 } }, // Left (-X)
        { 1, 2, 6, 5, { 1, 0,  0 } }, // Right (+X)
        { 3, 7, 6, 2, { 0, 1,  0 } }, // Top (+Y)
        { 0, 1, 5, 4, { 0,-1,  0 } }  // Bottom (-Y)
    };

    for (int f = 0; f < 6; ++f) {
        uint32_t bIdx = (uint32_t)mesh.vertices.size();
        mesh.vertices.push_back({ corners[faces[f].i0], faces[f].n, { 0, 0 } });
        mesh.vertices.push_back({ corners[faces[f].i1], faces[f].n, { 0, 1 } });
        mesh.vertices.push_back({ corners[faces[f].i2], faces[f].n, { 1, 1 } });
        mesh.vertices.push_back({ corners[faces[f].i3], faces[f].n, { 1, 0 } });

        mesh.indices.push_back(bIdx);
        mesh.indices.push_back(bIdx + 1);
        mesh.indices.push_back(bIdx + 2);

        mesh.indices.push_back(bIdx);
        mesh.indices.push_back(bIdx + 2);
        mesh.indices.push_back(bIdx + 3);
    }
}

Model3DMesh Model3DLoader::CreatePrimitive(const std::string& primitiveName) {
    Model3DMesh mesh;
    mesh.name = primitiveName;

    if (primitiveName == "Cruz 3D") {
        // Cruz Latina 3D compuesta por poste vertical y travesaño horizontal
        AddBoxToMesh(mesh, { -0.22f, -1.20f, -0.20f }, { 0.22f, 1.20f, 0.20f });
        AddBoxToMesh(mesh, { -0.85f,  0.25f, -0.19f }, { 0.85f, 0.65f, 0.19f });
    } else if (primitiveName == "Cubo") {
        AddBoxToMesh(mesh, { -0.75f, -0.75f, -0.75f }, { 0.75f, 0.75f, 0.75f });
    } else if (primitiveName == "Esfera") {
        const int stacks = 20, slices = 28;
        const float r = 0.9f;
        for (int i = 0; i <= stacks; ++i) {
            float phi = (float)i / stacks * 3.14159265f;
            for (int j = 0; j <= slices; ++j) {
                float theta = (float)j / slices * 2.0f * 3.14159265f;
                Vec3 p = {
                    r * std::sin(phi) * std::cos(theta),
                    r * std::cos(phi),
                    r * std::sin(phi) * std::sin(theta)
                };
                mesh.vertices.push_back({ p, p.Normalized(), { (float)j / slices, (float)i / stacks } });
            }
        }
        for (int i = 0; i < stacks; ++i) {
            for (int j = 0; j < slices; ++j) {
                uint32_t i0 = i * (slices + 1) + j;
                uint32_t i1 = (i + 1) * (slices + 1) + j;
                uint32_t i2 = (i + 1) * (slices + 1) + (j + 1);
                uint32_t i3 = i * (slices + 1) + (j + 1);

                mesh.indices.push_back(i0);
                mesh.indices.push_back(i1);
                mesh.indices.push_back(i2);

                mesh.indices.push_back(i0);
                mesh.indices.push_back(i2);
                mesh.indices.push_back(i3);
            }
        }
    } else if (primitiveName == "Cilindro") {
        const int slices = 24;
        const float r = 0.65f, h = 1.4f;
        for (int i = 0; i <= slices; ++i) {
            float theta = (float)i / slices * 2.0f * 3.14159265f;
            float cx = std::cos(theta), cz = std::sin(theta);
            Vec3 n = { cx, 0, cz };
            mesh.vertices.push_back({ { r * cx,  h * 0.5f, r * cz }, n, { (float)i / slices, 1 } });
            mesh.vertices.push_back({ { r * cx, -h * 0.5f, r * cz }, n, { (float)i / slices, 0 } });
        }
        for (int i = 0; i < slices; ++i) {
            uint32_t i0 = i * 2;
            uint32_t i1 = i * 2 + 1;
            uint32_t i2 = (i + 1) * 2 + 1;
            uint32_t i3 = (i + 1) * 2;

            mesh.indices.push_back(i0);
            mesh.indices.push_back(i1);
            mesh.indices.push_back(i2);

            mesh.indices.push_back(i0);
            mesh.indices.push_back(i2);
            mesh.indices.push_back(i3);
        }
    } else if (primitiveName == "Torus / Anillo") {
        const int mainSegs = 28, tubeSegs = 16;
        const float R = 0.70f, r = 0.25f;
        for (int i = 0; i <= mainSegs; ++i) {
            float u = (float)i / mainSegs * 2.0f * 3.14159265f;
            for (int j = 0; j <= tubeSegs; ++j) {
                float v = (float)j / tubeSegs * 2.0f * 3.14159265f;
                float x = (R + r * std::cos(v)) * std::cos(u);
                float y = r * std::sin(v);
                float z = (R + r * std::cos(v)) * std::sin(u);
                Vec3 p = { x, y, z };
                Vec3 c = { R * std::cos(u), 0, R * std::sin(u) };
                Vec3 n = (p - c).Normalized();
                mesh.vertices.push_back({ p, n, { (float)i / mainSegs, (float)j / tubeSegs } });
            }
        }
        for (int i = 0; i < mainSegs; ++i) {
            for (int j = 0; j < tubeSegs; ++j) {
                uint32_t i0 = i * (tubeSegs + 1) + j;
                uint32_t i1 = (i + 1) * (tubeSegs + 1) + j;
                uint32_t i2 = (i + 1) * (tubeSegs + 1) + (j + 1);
                uint32_t i3 = i * (tubeSegs + 1) + (j + 1);

                mesh.indices.push_back(i0);
                mesh.indices.push_back(i1);
                mesh.indices.push_back(i2);

                mesh.indices.push_back(i0);
                mesh.indices.push_back(i2);
                mesh.indices.push_back(i3);
            }
        }
    } else {
        // Pirámide por defecto
        Vec3 top = { 0, 0.9f, 0 };
        Vec3 b0 = { -0.8f, -0.6f, -0.8f };
        Vec3 b1 = {  0.8f, -0.6f, -0.8f };
        Vec3 b2 = {  0.8f, -0.6f,  0.8f };
        Vec3 b3 = { -0.8f, -0.6f,  0.8f };

        AddBoxToMesh(mesh, { -0.8f, -0.65f, -0.8f }, { 0.8f, -0.6f, 0.8f });
        auto AddTri = [&](Vec3 p0, Vec3 p1, Vec3 p2) {
            Vec3 n = Vec3::Cross(p1 - p0, p2 - p0).Normalized();
            uint32_t base = (uint32_t)mesh.vertices.size();
            mesh.vertices.push_back({ p0, n, { 0, 0 } });
            mesh.vertices.push_back({ p1, n, { 1, 0 } });
            mesh.vertices.push_back({ p2, n, { 0.5f, 1 } });
            mesh.indices.push_back(base);
            mesh.indices.push_back(base + 1);
            mesh.indices.push_back(base + 2);
        };
        AddTri(b0, b1, top);
        AddTri(b1, b2, top);
        AddTri(b2, b3, top);
        AddTri(b3, b0, top);
    }

    ComputeNormalsAndBounds(mesh);
    return mesh;
}

std::vector<std::string> Model3DLoader::GetAvailablePrimitives() {
    return {
        "Cubo"
    };
}

// ── glTF 2.0 & GLB Parser ───────────────────────────────────────────────────

static std::vector<uint8_t> DecodeBase64(const std::string& in) {
    std::vector<uint8_t> out;
    std::vector<int> T(256, -1);
    const char* b64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int i = 0; i < 64; i++) T[(unsigned char)b64chars[i]] = i;

    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (T[c] == -1) {
            if (c == '=') break;
            continue;
        }
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back((uint8_t)((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

static Mat4 QuatToMat4(float x, float y, float z, float w) {
    Mat4 res = Mat4::Identity();
    float xx = x * x, yy = y * y, zz = z * z;
    float xy = x * y, xz = x * z, yz = y * z;
    float wx = w * x, wy = w * y, wz = w * z;

    res.m[0] = 1.0f - 2.0f * (yy + zz);
    res.m[1] = 2.0f * (xy + wz);
    res.m[2] = 2.0f * (xz - wy);

    res.m[4] = 2.0f * (xy - wz);
    res.m[5] = 1.0f - 2.0f * (xx + zz);
    res.m[6] = 2.0f * (yz + wx);

    res.m[8] = 2.0f * (xz + wy);
    res.m[9] = 2.0f * (yz - wx);
    res.m[10] = 1.0f - 2.0f * (xx + yy);
    return res;
}

static bool ReadAccessorFloats(const json& j, const std::vector<std::vector<uint8_t>>& buffers, int accIdx, std::vector<float>& outFloats, int& outNumComponents) {
    if (accIdx < 0 || !j.contains("accessors") || accIdx >= (int)j["accessors"].size()) return false;
    const auto& acc = j["accessors"][accIdx];

    int bvIdx = acc.value("bufferView", -1);
    if (bvIdx < 0 || !j.contains("bufferViews") || bvIdx >= (int)j["bufferViews"].size()) return false;
    const auto& bv = j["bufferViews"][bvIdx];

    int bufIdx = bv.value("buffer", 0);
    if (bufIdx < 0 || bufIdx >= (int)buffers.size()) return false;
    const auto& buf = buffers[bufIdx];

    size_t bvByteOffset  = bv.value("byteOffset", (size_t)0);
    size_t accByteOffset = acc.value("byteOffset", (size_t)0);
    size_t totalOffset   = bvByteOffset + accByteOffset;
    size_t count         = acc.value("count", (size_t)0);
    int compType         = acc.value("componentType", 5126);
    std::string type     = acc.value("type", "SCALAR");

    int numComp = 1;
    if (type == "VEC2") numComp = 2;
    else if (type == "VEC3") numComp = 3;
    else if (type == "VEC4") numComp = 4;
    else if (type == "MAT4") numComp = 16;
    outNumComponents = numComp;

    size_t compSize = (compType == 5126 || compType == 5125) ? 4 : (compType == 5123 || compType == 5122) ? 2 : 1;
    size_t byteStride = bv.value("byteStride", numComp * compSize);
    if (byteStride == 0) byteStride = numComp * compSize;

    outFloats.resize(count * numComp);

    for (size_t i = 0; i < count; ++i) {
        const uint8_t* ptr = buf.data() + totalOffset + i * byteStride;
        for (int c = 0; c < numComp; ++c) {
            float val = 0.0f;
            if (compType == 5126) { // GL_FLOAT
                float f;
                std::memcpy(&f, ptr + c * 4, 4);
                val = f;
            } else if (compType == 5123) { // GL_UNSIGNED_SHORT
                uint16_t us;
                std::memcpy(&us, ptr + c * 2, 2);
                val = (float)us;
            } else if (compType == 5121) { // GL_UNSIGNED_BYTE
                uint8_t ub = *(ptr + c);
                val = (float)ub;
            }
            outFloats[i * numComp + c] = val;
        }
    }
    return true;
}

static bool ReadAccessorIndices(const json& j, const std::vector<std::vector<uint8_t>>& buffers, int accIdx, std::vector<uint32_t>& outIndices) {
    if (accIdx < 0 || !j.contains("accessors") || accIdx >= (int)j["accessors"].size()) return false;
    const auto& acc = j["accessors"][accIdx];

    int bvIdx = acc.value("bufferView", -1);
    if (bvIdx < 0 || !j.contains("bufferViews") || bvIdx >= (int)j["bufferViews"].size()) return false;
    const auto& bv = j["bufferViews"][bvIdx];

    int bufIdx = bv.value("buffer", 0);
    if (bufIdx < 0 || bufIdx >= (int)buffers.size()) return false;
    const auto& buf = buffers[bufIdx];

    size_t bvByteOffset  = bv.value("byteOffset", (size_t)0);
    size_t accByteOffset = acc.value("byteOffset", (size_t)0);
    size_t totalOffset   = bvByteOffset + accByteOffset;
    size_t count         = acc.value("count", (size_t)0);
    int compType         = acc.value("componentType", 5123);

    size_t compSize = (compType == 5125) ? 4 : (compType == 5123 || compType == 5122) ? 2 : 1;
    size_t byteStride = bv.value("byteStride", compSize);
    if (byteStride == 0) byteStride = compSize;

    outIndices.resize(count);

    for (size_t i = 0; i < count; ++i) {
        const uint8_t* ptr = buf.data() + totalOffset + i * byteStride;
        if (compType == 5125) { // GL_UNSIGNED_INT
            uint32_t ui;
            std::memcpy(&ui, ptr, 4);
            outIndices[i] = ui;
        } else if (compType == 5123) { // GL_UNSIGNED_SHORT
            uint16_t us;
            std::memcpy(&us, ptr, 2);
            outIndices[i] = (uint32_t)us;
        } else if (compType == 5121) { // GL_UNSIGNED_BYTE
            outIndices[i] = (uint32_t)(*ptr);
        }
    }
    return true;
}

static bool ParseGLTFJson(const json& j, const std::vector<std::vector<uint8_t>>& buffers, Model3DMesh& outMesh) {
    if (!j.contains("meshes") || !j["meshes"].is_array() || j["meshes"].empty()) return false;

    // Función recursiva para procesar nodos y aplicar matrices de transformación
    std::function<void(int, const Mat4&)> ProcessNode = [&](int nodeIdx, const Mat4& parentMat) {
        if (!j.contains("nodes") || nodeIdx < 0 || nodeIdx >= (int)j["nodes"].size()) return;
        const auto& node = j["nodes"][nodeIdx];

        Mat4 localMat = Mat4::Identity();
        if (node.contains("matrix") && node["matrix"].is_array() && node["matrix"].size() == 16) {
            for (int k = 0; k < 16; ++k) localMat.m[k] = node["matrix"][k].get<float>();
        } else {
            if (node.contains("translation") && node["translation"].size() == 3) {
                localMat = localMat * Mat4::Translation({
                    node["translation"][0].get<float>(),
                    node["translation"][1].get<float>(),
                    node["translation"][2].get<float>()
                });
            }
            if (node.contains("rotation") && node["rotation"].size() == 4) {
                localMat = localMat * QuatToMat4(
                    node["rotation"][0].get<float>(),
                    node["rotation"][1].get<float>(),
                    node["rotation"][2].get<float>(),
                    node["rotation"][3].get<float>()
                );
            }
            if (node.contains("scale") && node["scale"].size() == 3) {
                localMat = localMat * Mat4::Scale({
                    node["scale"][0].get<float>(),
                    node["scale"][1].get<float>(),
                    node["scale"][2].get<float>()
                });
            }
        }

        Mat4 worldMat = parentMat * localMat;

        if (node.contains("mesh")) {
            int meshIdx = node["mesh"].get<int>();
            if (meshIdx >= 0 && meshIdx < (int)j["meshes"].size()) {
                const auto& meshObj = j["meshes"][meshIdx];
                if (meshObj.contains("primitives") && meshObj["primitives"].is_array()) {
                    for (const auto& prim : meshObj["primitives"]) {
                        if (!prim.contains("attributes")) continue;
                        const auto& attrs = prim["attributes"];
                        if (!attrs.contains("POSITION")) continue;

                        int posAcc = attrs["POSITION"].get<int>();
                        std::vector<float> positions;
                        int nComp = 3;
                        if (!ReadAccessorFloats(j, buffers, posAcc, positions, nComp) || positions.empty()) continue;

                        std::vector<float> normals;
                        if (attrs.contains("NORMAL")) {
                            int normAcc = attrs["NORMAL"].get<int>();
                            ReadAccessorFloats(j, buffers, normAcc, normals, nComp);
                        }

                        std::vector<float> uvs;
                        if (attrs.contains("TEXCOORD_0")) {
                            int uvAcc = attrs["TEXCOORD_0"].get<int>();
                            ReadAccessorFloats(j, buffers, uvAcc, uvs, nComp);
                        }

                        size_t vertCount = positions.size() / 3;
                        uint32_t baseVertexIdx = (uint32_t)outMesh.vertices.size();

                        for (size_t v = 0; v < vertCount; ++v) {
                            Vec3 rawP = { positions[v * 3], positions[v * 3 + 1], positions[v * 3 + 2] };
                            Vec4 transformedP = worldMat * Vec4(rawP, 1.0f);
                            Vec3 p = { transformedP.x, transformedP.y, transformedP.z };

                            Vec3 n = { 0, 1, 0 };
                            if (normals.size() >= (v + 1) * 3) {
                                Vec3 rawN = { normals[v * 3], normals[v * 3 + 1], normals[v * 3 + 2] };
                                Vec4 transformedN = worldMat * Vec4(rawN, 0.0f);
                                n = Vec3(transformedN.x, transformedN.y, transformedN.z).Normalized();
                            }

                            Vec2 uv = { 0, 0 };
                            if (uvs.size() >= (v + 1) * 2) {
                                uv = { uvs[v * 2], uvs[v * 2 + 1] };
                            }

                            outMesh.vertices.push_back({ p, n, uv });
                        }

                        if (prim.contains("indices")) {
                            int indAcc = prim["indices"].get<int>();
                            std::vector<uint32_t> inds;
                            if (ReadAccessorIndices(j, buffers, indAcc, inds)) {
                                for (uint32_t idx : inds) {
                                    outMesh.indices.push_back(baseVertexIdx + idx);
                                }
                            }
                        } else {
                            for (size_t v = 0; v < vertCount; ++v) {
                                outMesh.indices.push_back(baseVertexIdx + (uint32_t)v);
                            }
                        }
                    }
                }
            }
        }

        if (node.contains("children") && node["children"].is_array()) {
            for (const auto& child : node["children"]) {
                ProcessNode(child.get<int>(), worldMat);
            }
        }
    };

    if (j.contains("scenes") && j["scenes"].is_array() && !j["scenes"].empty()) {
        int sceneIdx = j.value("scene", 0);
        if (sceneIdx < 0 || sceneIdx >= (int)j["scenes"].size()) sceneIdx = 0;
        const auto& sceneObj = j["scenes"][sceneIdx];
        if (sceneObj.contains("nodes") && sceneObj["nodes"].is_array()) {
            for (const auto& nIdx : sceneObj["nodes"]) {
                ProcessNode(nIdx.get<int>(), Mat4::Identity());
            }
        }
    } else if (j.contains("nodes") && j["nodes"].is_array()) {
        for (int i = 0; i < (int)j["nodes"].size(); ++i) {
            ProcessNode(i, Mat4::Identity());
        }
    }

    if (outMesh.vertices.empty() || outMesh.indices.empty()) return false;

    Model3DLoader::ComputeNormalsAndBounds(outMesh);
    return true;
}

bool Model3DLoader::LoadGLTF(const std::string& filePath, Model3DMesh& outMesh) {
    std::ifstream file(filePath);
    if (!file.is_open()) return false;

    json j;
    try {
        file >> j;
    } catch (...) {
        return false;
    }

    outMesh.vertices.clear();
    outMesh.indices.clear();
    outMesh.name = fs::path(filePath).stem().string();

    fs::path baseDir = fs::path(filePath).parent_path();
    std::vector<std::vector<uint8_t>> buffers;

    if (j.contains("buffers") && j["buffers"].is_array()) {
        buffers.resize(j["buffers"].size());
        for (size_t i = 0; i < j["buffers"].size(); ++i) {
            const auto& bufObj = j["buffers"][i];
            if (!bufObj.contains("uri")) continue;

            std::string uri = bufObj["uri"].get<std::string>();
            if (uri.rfind("data:", 0) == 0) {
                size_t commaPos = uri.find(',');
                if (commaPos != std::string::npos) {
                    buffers[i] = DecodeBase64(uri.substr(commaPos + 1));
                }
            } else {
                fs::path binPath = baseDir / uri;
                std::ifstream binFile(binPath, std::ios::binary);
                if (binFile.is_open()) {
                    binFile.seekg(0, std::ios::end);
                    size_t sz = binFile.tellg();
                    binFile.seekg(0, std::ios::beg);
                    buffers[i].resize(sz);
                    binFile.read((char*)buffers[i].data(), sz);
                }
            }
        }
    }

    return ParseGLTFJson(j, buffers, outMesh);
}

bool Model3DLoader::LoadGLB(const std::string& filePath, Model3DMesh& outMesh) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;

    outMesh.vertices.clear();
    outMesh.indices.clear();
    outMesh.name = fs::path(filePath).stem().string();

    uint32_t magic = 0, version = 0, length = 0;
    file.read((char*)&magic, 4);
    file.read((char*)&version, 4);
    file.read((char*)&length, 4);

    if (magic != 0x46546C67 || version != 2) return false;

    json j;
    std::vector<std::vector<uint8_t>> buffers;
    buffers.resize(1);

    while (file.tellg() < (std::streampos)length && !file.eof()) {
        uint32_t chunkLength = 0, chunkType = 0;
        file.read((char*)&chunkLength, 4);
        file.read((char*)&chunkType, 4);
        if (file.gcount() < 8) break;

        if (chunkType == 0x4E4F534A) { // "JSON"
            std::string jsonStr(chunkLength, '\0');
            file.read(&jsonStr[0], chunkLength);
            try {
                j = json::parse(jsonStr);
            } catch (...) {
                return false;
            }
        } else if (chunkType == 0x004E4942) { // "BIN\0"
            buffers[0].resize(chunkLength);
            file.read((char*)buffers[0].data(), chunkLength);
        } else {
            file.seekg(chunkLength, std::ios::cur);
        }
    }

    return ParseGLTFJson(j, buffers, outMesh);
}

// ── General Loader and Directory Scanner ────────────────────────────────────

bool Model3DLoader::LoadModel(const std::string& filePath, Model3DMesh& outMesh, Model3DFormat& outFormat) {
    if (filePath.empty()) return false;

    std::string ext = fs::path(filePath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".obj") {
        outFormat = Model3DFormat::OBJ;
        return LoadOBJ(filePath, outMesh);
    } else if (ext == ".stl") {
        outFormat = Model3DFormat::STL;
        return LoadSTL(filePath, outMesh);
    } else if (ext == ".ply") {
        outFormat = Model3DFormat::PLY;
        return LoadPLY(filePath, outMesh);
    } else if (ext == ".gltf") {
        outFormat = Model3DFormat::GLTF;
        return LoadGLTF(filePath, outMesh);
    } else if (ext == ".glb") {
        outFormat = Model3DFormat::GLB;
        return LoadGLB(filePath, outMesh);
    }

    return false;
}

std::vector<Model3DAsset> Model3DLoader::ScanDirectory(const std::string& folderPath) {
    std::vector<Model3DAsset> results;

    // Agregar primitivas integradas primero
    for (const auto& primName : GetAvailablePrimitives()) {
        Model3DAsset a;
        a.path          = "[built-in]:" + primName;
        a.displayName   = primName;
        a.folder        = "Integrados";
        a.format        = Model3DFormat::Primitive;
        a.isBuiltIn     = true;
        a.fileSizeBytes = 0;
        results.push_back(a);
    }

    if (folderPath.empty() || !fs::exists(folderPath)) return results;

    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(folderPath, ec)) {
        if (!entry.is_regular_file()) continue;

        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        Model3DFormat fmt;
        if (ext == ".obj") fmt = Model3DFormat::OBJ;
        else if (ext == ".stl") fmt = Model3DFormat::STL;
        else if (ext == ".ply") fmt = Model3DFormat::PLY;
        else if (ext == ".gltf") fmt = Model3DFormat::GLTF;
        else if (ext == ".glb") fmt = Model3DFormat::GLB;
        else continue;

        Model3DAsset asset;
        asset.path          = entry.path().string();
        
        // Si el archivo se llama "scene.gltf" o "model.gltf", usar el nombre de la carpeta contenedora
        std::string stemName = entry.path().stem().string();
        if ((stemName == "scene" || stemName == "model") && entry.path().parent_path() != fs::path(folderPath)) {
            asset.displayName = entry.path().parent_path().filename().string();
        } else {
            asset.displayName = stemName;
        }

        asset.folder        = entry.path().parent_path().string();
        asset.format        = fmt;
        asset.fileSizeBytes = entry.file_size(ec);
        asset.isBuiltIn     = false;

        results.push_back(asset);
    }

    return results;
}

} // namespace ProyecThor::UI


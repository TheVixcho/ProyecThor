#pragma once
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <imgui.h>

namespace ProyecThor::UI {

// ── Álgebra Lineal 3D pura y compacta ────────────────────────────────────────

struct Vec2 {
    float x = 0.0f, y = 0.0f;
    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}
};

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;

    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vec3 operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vec3 operator*(float s) const       { return { x * s, y * s, z * s }; }
    Vec3 operator/(float s) const       { float inv = 1.0f / s; return { x * inv, y * inv, z * inv }; }

    Vec3& operator+=(const Vec3& o)     { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o)     { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s)           { x *= s; y *= s; z *= s; return *this; }

    float LengthSq() const { return x * x + y * y + z * z; }
    float Length() const   { return std::sqrt(LengthSq()); }

    Vec3 Normalized() const {
        float l = Length();
        if (l > 1e-6f) return *this * (1.0f / l);
        return { 0.0f, 1.0f, 0.0f };
    }

    static float Dot(const Vec3& a, const Vec3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    static Vec3 Cross(const Vec3& a, const Vec3& b) {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }
};

struct Vec4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;
    Vec4() = default;
    Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    Vec4(const Vec3& v, float w_ = 1.0f) : x(v.x), y(v.y), z(v.z), w(w_) {}
};

// Matriz 4x4 en orden columna (OpenGL column-major)
struct Mat4 {
    float m[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    Mat4() = default;

    static Mat4 Identity() {
        return Mat4();
    }

    Mat4 operator*(const Mat4& r) const {
        Mat4 res;
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                res.m[col * 4 + row] =
                    m[0 * 4 + row] * r.m[col * 4 + 0] +
                    m[1 * 4 + row] * r.m[col * 4 + 1] +
                    m[2 * 4 + row] * r.m[col * 4 + 2] +
                    m[3 * 4 + row] * r.m[col * 4 + 3];
            }
        }
        return res;
    }

    Vec4 operator*(const Vec4& v) const {
        return {
            m[0] * v.x + m[4] * v.y + m[8]  * v.z + m[12] * v.w,
            m[1] * v.x + m[5] * v.y + m[9]  * v.z + m[13] * v.w,
            m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14] * v.w,
            m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15] * v.w
        };
    }

    static Mat4 Translation(const Vec3& t) {
        Mat4 res = Identity();
        res.m[12] = t.x;
        res.m[13] = t.y;
        res.m[14] = t.z;
        return res;
    }

    static Mat4 Scale(const Vec3& s) {
        Mat4 res = Identity();
        res.m[0]  = s.x;
        res.m[5]  = s.y;
        res.m[10] = s.z;
        return res;
    }

    static Mat4 RotationX(float rad) {
        Mat4 res = Identity();
        float c = std::cos(rad), s = std::sin(rad);
        res.m[5] = c;  res.m[9] = -s;
        res.m[6] = s;  res.m[10] = c;
        return res;
    }

    static Mat4 RotationY(float rad) {
        Mat4 res = Identity();
        float c = std::cos(rad), s = std::sin(rad);
        res.m[0] = c;   res.m[8] = s;
        res.m[2] = -s;  res.m[10] = c;
        return res;
    }

    static Mat4 RotationZ(float rad) {
        Mat4 res = Identity();
        float c = std::cos(rad), s = std::sin(rad);
        res.m[0] = c;  res.m[4] = -s;
        res.m[1] = s;  res.m[5] = c;
        return res;
    }

    static Mat4 Perspective(float fovYRad, float aspect, float zNear, float zFar) {
        Mat4 res;
        std::fill(std::begin(res.m), std::end(res.m), 0.0f);
        float tanHalfFov = std::tan(fovYRad * 0.5f);
        res.m[0]  = 1.0f / (aspect * tanHalfFov);
        res.m[5]  = 1.0f / tanHalfFov;
        res.m[10] = -(zFar + zNear) / (zFar - zNear);
        res.m[11] = -1.0f;
        res.m[14] = -(2.0f * zFar * zNear) / (zFar - zNear);
        return res;
    }

    static Mat4 LookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
        Vec3 f = (target - eye).Normalized();
        Vec3 s = Vec3::Cross(f, up).Normalized();
        Vec3 u = Vec3::Cross(s, f);

        Mat4 res = Identity();
        res.m[0] = s.x;  res.m[4] = s.y;  res.m[8]  = s.z;
        res.m[1] = u.x;  res.m[5] = u.y;  res.m[9]  = u.z;
        res.m[2] = -f.x; res.m[6] = -f.y; res.m[10] = -f.z;
        res.m[12] = -Vec3::Dot(s, eye);
        res.m[13] = -Vec3::Dot(u, eye);
        res.m[14] =  Vec3::Dot(f, eye);
        return res;
    }
};

// ── Estructuras de Malla y Modelo 3D ─────────────────────────────────────────

struct Model3DVertex {
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
};

struct Model3DMesh {
    std::string                name;
    std::vector<Model3DVertex> vertices;
    std::vector<uint32_t>      indices;

    Vec3 bboxMin = { 0, 0, 0 };
    Vec3 bboxMax = { 0, 0, 0 };
    Vec3 center  = { 0, 0, 0 };
    float radius = 1.0f;

    // Recursos OpenGL
    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int ebo = 0;
    bool isGpuLoaded  = false;

    void UploadToGpu();
    void ReleaseGpu();
};

enum class Model3DFormat {
    OBJ,
    STL,
    PLY,
    GLTF,
    GLB,
    Primitive
};

struct Model3DAsset {
    std::string    path;
    std::string    displayName;
    std::string    folder;
    Model3DFormat  format = Model3DFormat::OBJ;
    uintmax_t      fileSizeBytes = 0;
    size_t         vertexCount   = 0;
    size_t         triangleCount = 0;
    bool           isBuiltIn     = false;
};

enum class Model3DShading {
    SmoothLit,
    Wireframe,
    ShadedWithEdges,
    FlatNormals
};

enum class Model3DBackground {
    Transparent,
    DarkStudio,
    GradientBlue,
    SolidBlack
};

struct Model3DRenderConfig {
    Model3DShading    shading    = Model3DShading::SmoothLit;
    Model3DBackground background = Model3DBackground::Transparent;

    ImVec4 modelColor      = { 0.92f, 0.93f, 0.98f, 1.0f };
    ImVec4 wireframeColor  = { 0.38f, 0.72f, 1.00f, 0.85f };
    float  wireframeWidth  = 1.2f;

    bool   autoRotate      = false;
    float  autoRotateSpeed = 35.0f; // grados por segundo

    bool   showGrid        = true;
    bool   showLighting    = true;
    float  lightIntensity  = 1.15f;
    float  ambientIntensity= 0.35f;
    Vec3   lightDir        = { 0.55f, 0.85f, 0.45f };

    // Cámara interactiva (Orbital)
    float  cameraYaw       = 45.0f;  // Grados horizontal
    float  cameraPitch     = 25.0f;  // Grados vertical (-89 .. 89)
    float  cameraDistance  = 3.2f;   // Distancia al centro
    Vec3   cameraTarget    = { 0.0f, 0.0f, 0.0f }; // Pan offset
    bool   flipY           = false;  // Invertir orientación vertical del modelo

    void ResetCamera() {
        cameraYaw      = 45.0f;
        cameraPitch    = 25.0f;
        cameraDistance = 3.2f;
        cameraTarget   = { 0.0f, 0.0f, 0.0f };
        flipY          = false;
    }
};

} // namespace ProyecThor::UI

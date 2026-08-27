#pragma once
#include <cstdint>
#include <jni.h>
#include <functional>

struct Vector3 { float x, y, z; };
struct Vector2 { float x, y; };
struct Matrix4x4 { float m[16]; };

class ESP {
public:
    bool m_init = false;
    uintptr_t m_libBase = 0;
    float m_width = 1080.0f;
    float m_height = 2340.0f;

    bool Init();
    uintptr_t GetBase();
    uintptr_t GetLocalPlayer();
    bool IsDead(uintptr_t e);
    bool IsTeammate(uintptr_t e);
    bool IsVisible(uintptr_t e);
    Vector3 GetBonePosition(uintptr_t e, uint32_t bone);
    bool WorldToScreen(Vector3 world, Vector2& screen);
    void DrawBox(float x, float y, float w, float h, uint32_t color);
    void DrawLine(float x1, float y1, float x2, float y2, uint32_t color);
    void RenderLoop();

    static GLuint shader, vao, vbo;
};

extern ESP g_ESP;
extern bool g_ESP_Enabled;

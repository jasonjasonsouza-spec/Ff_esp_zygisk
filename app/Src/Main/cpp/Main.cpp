#include "esp.h"
#include "offsets.h"
#include "And64InlineHook.h"
#include <jni.h>
#include <dlfcn.h>
#include <android/log.h>
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>

#define LOG_TAG "FF_ZYGISK_ESP"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ==============================================
// 🎛️ CONTROLE — VOLUME MENOS (-) Liga/Desliga
// ==============================================
bool g_ESP_Enabled = true;   // ← Troca pra false pra começar desligado
static bool s_lastVolDown = false;

static void CheckVolumeButton() {
    int fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
    if (fd < 0) fd = open("/dev/input/event1", O_RDONLY | O_NONBLOCK);
    if (fd < 0) fd = open("/dev/input/event2", O_RDONLY | O_NONBLOCK);
    if (fd < 0) return;

    input_event e;
    ssize_t n = read(fd, &e, sizeof(e));
    close(fd);

    if (n == sizeof(e) && e.type == EV_KEY && e.code == KEY_VOLUMEDOWN) {
        if (e.value == 1 && !s_lastVolDown) {
            g_ESP_Enabled = !g_ESP_Enabled;
            LOGI("🎛️ ESP: %s", g_ESP_Enabled ? "✅ ATIVADO" : "❌ DESATIVADO");
        }
        s_lastVolDown = (e.value == 1);
    }
}

// ==============================================
// 📋 LISTA DE JOGADORES — DICTIONARY IL2CPP
// ==============================================
static void ForEachEntity(std::function<void(uintptr_t)> callback) {
    uintptr_t base = g_ESP.GetBase();
    uintptr_t dict = *(uintptr_t*)((void*)(base + ENTITY_DICTIONARY));
    if (!dict) return;

    uintptr_t pairs = *(uintptr_t*)((void*)(dict + DICT_PAIRS_OFFSET));
    int32_t count = *(int32_t*)((void*)(dict + DICT_COUNT_OFFSET));
    if (!pairs || count <= 0 || count > 300) return;

    for (int i = 0; i < count; i++) {
        uintptr_t entity = *(uintptr_t*)((void*)(pairs + i * 0x10 + DICT_ENTITY_OFFSET));
        if (entity) callback(entity);
    }
}

// ==============================================
// CLASSE ESP — IMPLEMENTAÇÃO
// ==============================================
GLuint ESP::shader = 0;
GLuint ESP::vao = 0;
GLuint ESP::vbo = 0;

bool ESP::Init() {
    if (m_init) return true;
    void* lib = dlopen("libil2cpp.so", RTLD_LAZY);
    if (!lib) { LOGE("❌ libil2cpp.so não encontrada!"); return false; }
    m_libBase = (uintptr_t)dlsym(lib, "JNI_OnLoad") - 0x3A84000;
    dlclose(lib);
    LOGI("✅ libil2cpp base: 0x%lx", m_libBase);
    m_init = true;
    return true;
}

uintptr_t ESP::GetBase() { return m_libBase + INIT_BASE; }
uintptr_t ESP::GetLocalPlayer() {
    uintptr_t match = *(uintptr_t*)((void*)(GetBase() + CURRENT_MATCH));
    return match ? *(uintptr_t*)((void*)(match + LOCAL_PLAYER)) : 0;
}
bool ESP::IsDead(uintptr_t e) { return *(bool*)((void*)(e + PLAYER_IS_DEAD)); }
bool ESP::IsTeammate(uintptr_t e) { return *(bool*)((void*)(e + PLAYER_IS_TEAMMATE)); }
bool ESP::IsVisible(uintptr_t e) {
    uintptr_t av = *(uintptr_t*)((void*)(e + AVATAR_INSTANCE));
    return av ? *(bool*)((void*)(av + AVATAR_IS_VISIBLE)) : true;
}
Vector3 ESP::GetBonePosition(uintptr_t e, uint32_t bone) {
    uintptr_t node = *(uintptr_t*)((void*)(e + bone));
    return node ? *(Vector3*)((void*)(node + TRANSFORM_NODE_POS)) : Vector3{0,0,0};
}
bool ESP::WorldToScreen(Vector3 world, Vector2& screen) {
    uintptr_t cam = *(uintptr_t*)((void*)(GetBase() + CAMERA_TRANSFORM));
    if (!cam) return false;
    Matrix4x4 mat = *(Matrix4x4*)((void*)(cam + MATRIX_WORLD_TO_SCREEN));

    float w = mat.m[3] * world.x + mat.m[7] * world.y + mat.m[11] * world.z + mat.m[15];
    if (w < 0.001f) return false;

    float x = mat.m[0] * world.x + mat.m[4] * world.y + mat.m[8] * world.z + mat.m[12];
    float y = mat.m[1] * world.x + mat.m[5] * world.y + mat.m[9] * world.z + mat.m[13];

    screen.x = (m_width * 0.5f) * (1.0f - x / w) + m_width * 0.5f;
    screen.y = (m_height * 0.5f) * (1.0f - y / w) + m_height * 0.5f;
    return true;
}

// === DESENHO ===
static void CompileShaders() {
    const char* vs = "#version 300 es\nprecision mediump float;\nin vec2 p;\nin vec4 c;\nout vec4 vc;\nvoid main(){gl_Position=vec4(p,0,1);vc=c;}";
    const char* fs = "#version 300 es\nprecision mediump float;\nin vec4 vc;\nout vec4 o;\nvoid main(){o=vc;}";

    GLuint v = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(v, 1, &vs, 0); glCompileShader(v);
    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f, 1, &fs, 0); glCompileShader(f);

    ESP::shader = glCreateProgram();
    glAttachShader(ESP::shader, v); glAttachShader(ESP::shader, f);
    glLinkProgram(ESP::shader);
    glDeleteShader(v); glDeleteShader(f);

    glGenVertexArrays(1, &ESP::vao);
    glGenBuffers(1, &ESP::vbo);
}

void ESP::DrawBox(float x, float y, float w, float h, uint32_t color) {
    if (!shader) CompileShaders();
    float r = ((color>>16)&0xFF)/255.0f, g = ((color>>8)&0xFF)/255.0f;
    float b = (color&0xFF)/255.0f, a = ((color>>24)&0xFF)/255.0f;
    float sx = (x/m_width)*2-1, sy = 1-(y/m_height)*2;
    float sw = (w/m_width)*2, sh = (h/m_height)*2;

    float v[] = {
        sx, sy, r,g,b,a, sx+sw, sy, r,g,b,a,
        sx+sw, sy, r,g,b,a, sx+sw, sy+sh, r,g,b,a,
        sx+sw, sy+sh, r,g,b,a, sx, sy+sh, r,g,b,a,
        sx, sy+sh, r,g,b,a, sx, sy, r,g,b,a,
    };
    glUseProgram(shader); glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,2,GL_FLOAT,0,24,0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,4,GL_FLOAT,0,24,(void*)8);
    glDrawArrays(GL_LINES, 0, 8);
}

void ESP::DrawLine(float x1, float y1, float x2, float y2, uint32_t color) {
    if (!shader) CompileShaders();
    float r = ((color>>16)&0xFF)/255.0f, g = ((color>>8)&0xFF)/255.0f;
    float b = (color&0xFF)/255.0f, a = ((color>>24)&0xFF)/255.0f;
    float sx1 = (x1/m_width)*2-1, sy1 = 1-(y1/m_height)*2;
    float sx2 = (x2/m_width)*2-1, sy2 = 1-(y2/m_height)*2;
    float v[] = {sx1,sy1,r,g,b,a, sx2,sy2,r,g,b,a};
    glUseProgram(shader); glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,2,GL_FLOAT,0,24,0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,4,GL_FLOAT,0,24,(void*)8);
    glDrawArrays(GL_LINES, 0, 2);
}

void ESP::RenderLoop() {
    if (!g_ESP_Enabled) return;
    if (!m_init && !Init()) return;

    uintptr_t local = GetLocalPlayer();
    if (!local) return;

    ForEachEntity([&](uintptr_t entity) {
        if (!entity || entity == local) return;
        if (IsDead(entity)) return;
        if (!IsVisible(entity)) return;

        uint32_t color = IsTeammate(entity) ? 0xFF00FF00 : 0xFFFF0000;

        Vector3 head = GetBonePosition(entity, BONE_HEAD);
        Vector3 hip = GetBonePosition(entity, BONE_HIP);
        Vector3 foot = GetBonePosition(entity, BONE_RIGHT_FOOT);

        Vector2 hs, hp, fs;
        if (!WorldToScreen(head, hs)) return;
        if (!WorldToScreen(hip, hp)) return;
        if (!WorldToScreen(foot, fs)) return;

        float h = fs.y - hs.y;
        float w = h * 0.6f;
        DrawBox(hp.x - w/2, hs.y, w, h, color);
        DrawLine(hp.x, fs.y, hp.x, m_height, color);
    });
}

ESP g_ESP;

// ==============================================
// 🪝 HOOK EGL
// ==============================================
typedef EGLBoolean (*eglSwapBuffers_t)(EGLDisplay, EGLSurface);
eglSwapBuffers_t o_eglSwap = nullptr;

EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surf) {
    CheckVolumeButton();
    if (g_ESP_Enabled) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);
        g_ESP.RenderLoop();
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }
    return o_eglSwap(dpy, surf);
}

// ==============================================
// 🚀 ENTRADA PRINCIPAL
// ==============================================
__attribute__((visibility("default")))
JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    (void)vm; (void)reserved;
    LOGI("========================================");
    LOGI("  🎮 Free Fire ESP — Zygisk Module");
    LOGI("  📦 Versão: 2.130.1");
    LOGI("========================================");
    LOGI("🎛️ VOLUME MENOS (-) → Liga/Desliga ESP");
    LOGI("📌 Estado inicial: %s", g_ESP_Enabled ? "✅ ATIVADO" : "❌ DESATIVADO");

    void* libEGL = dlopen("libEGL.so", RTLD_NOW);
    if (libEGL) {
        o_eglSwap = (eglSwapBuffers_t)dlsym(libEGL, "eglSwapBuffers");
        if (o_eglSwap) {
            LOGI("✅ eglSwapBuffers encontrado — instalando hook...");
            A64HookFunction((void*)o_eglSwap, (void*)hook_eglSwapBuffers, (void**)&o_eglSwap);
            LOGI("✅ HOOK INSTALADO — ESP PRONTO!");
        } else LOGE("❌ Não encontrou eglSwapBuffers!");
        dlclose(libEGL);
    } else LOGE("❌ Não abriu libEGL.so!");

    return JNI_VERSION_1_6;
}

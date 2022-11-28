#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <math.h> // sinf().
#include <stdint.h>
#include <stdio.h>
#include <windowsx.h>
#include <wingdi.h>
#include <winuser.h>

#include <GL/glew.h>
#include "wglext.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/imgui_impl_win32.h"

#include "assimp/Importer.hpp"
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#define global_variable static
#define internal static
#define local_persist static

#ifndef NDEBUG
#define myAssert(x)                                                                                                    \
    if (!(x))                                                                                                          \
    {                                                                                                                  \
        __debugbreak();                                                                                                \
    }
#else
#define myAssert(x)
#endif

#define myArraySize(arr) (sizeof((arr)) / sizeof((arr[0])))

#define max(x, y) (((x) > (y)) ? (x) : (y))

#define min(x, y) (((x) < (y)) ? (x) : (y))

#define clamp(x, min, max) (((x) < (min)) ? (min) : ((x) > (max)) ? (max) : (x))

typedef unsigned char uchar;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef float f32;
typedef double f64;

#define PI 3.1415926535f
#define NUM_POINTLIGHTS 4
#define NUM_OBJECTS 5

struct DirLight
{
    glm::vec3 direction;

    glm::vec3 ambient = glm::vec3(.05f);
    glm::vec3 diffuse = glm::vec3(.5f);
    glm::vec3 specular = glm::vec3(1.f);
};

struct PointLight
{
    glm::vec3 position;

    glm::vec3 ambient = glm::vec3(.05f);
    glm::vec3 diffuse = glm::vec3(.5f);
    glm::vec3 specular = glm::vec3(1.f);

    s32 attIndex = 4;
};

struct Attenuation
{
    u32 range;
    f32 linear = .7f;
    f32 quadratic = 1.8f;
};

Attenuation globalAttenuationTable[] = {
    {7, .7f, 1.8f},       {13, .35f, .44f},     {20, .22f, .20f},     {32, .14f, .07f},
    {50, .09f, .032f},    {65, .07f, .017f},    {100, .045f, .0075f}, {160, .027f, .0028f},
    {200, .022f, .0019f}, {325, .014f, .0007f}, {600, .007f, .0002f}, {3250, .0014f, .00007f},
};

struct SpotLight
{
    glm::vec3 position;
    glm::vec3 direction;

    glm::vec3 ambient = glm::vec3(0.f);
    glm::vec3 diffuse = glm::vec3(.5f);
    glm::vec3 specular = glm::vec3(1.f);

    float innerCutoff = PI / 11.f;
    float outerCutoff = PI / 9.f;
};

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
};

enum class TextureType
{
    Diffuse,
    Specular
};

struct Texture
{
    u32 id;
    TextureType type;
    u64 hash;
};

struct Mesh
{
    Vertex *vertices;
    u32 verticesSize;
    u32 *indices;
    u32 indicesSize;
    Texture *textures;
    u32 numTextures;
};

struct Model
{
    Mesh *meshes;
    u32 meshCount;
    u32 *vaos;
};

struct VaoInformation
{
    f32 *vertices;
    u32 verticesSize;
    s32 *elemCounts;
    u32 elementCountsSize;
    u32 *indices;
    u32 indicesSize;
};

struct ShaderProgram
{
    u32 id;
    char vertexShaderFilename[64];
    FILETIME vertexShaderTime;
    char geometryShaderFilename[64];
    FILETIME geometryShaderTime;
    char fragmentShaderFilename[64];
    FILETIME fragmentShaderTime;
};

struct TransientDrawingInfo
{
    ShaderProgram objectShader;
    ShaderProgram instancedObjectShader;
    ShaderProgram lightShader;
    ShaderProgram outlineShader;
    ShaderProgram glassShader;
    ShaderProgram textureShader;
    ShaderProgram postProcessShader;
    ShaderProgram skyboxShader;
    ShaderProgram geometryShader;
    
    u32 matricesUBO;

    u32 cubeVao;

    Model backpack;
    Model planet;
    Model asteroid;
    u32 grassTexture;
    u32 windowTexture;
    u32 skyboxTexture;

    s32 numSamples;
    
    u32 mainQuadVao;
    u32 mainFBO;
    u32 mainQuad;
    u32 mainRBO;

    u32 rearViewQuadVao;
    u32 rearViewFBO;
    u32 rearViewQuad;
    u32 rearViewRBO;
    
    u32 postProcessingQuadVao;
    u32 postProcessingFBO;
    u32 postProcessingQuad;
    u32 postProcessingRBO;
    
    u32 depthMapQuadVao;
    u32 depthMapFBO;
    u32 depthMapQuad;
    u32 depthMapRBO;
};

struct PersistentDrawingInfo
{
    bool initialized;
    bool wireframeMode = false;

    float clearColor[4] = {.1f, .1f, .1f, 1.f};
    DirLight dirLight;
    PointLight pointLights[NUM_POINTLIGHTS];
    SpotLight spotLight;
    glm::vec3 texCubePos[NUM_OBJECTS];
    glm::vec3 windowPos[NUM_OBJECTS];
    
    f32 materialShininess = 32.f;
    bool blinn = true;
    
    f32 gamma = 2.2f;
};

struct CameraInfo
{
    glm::vec3 pos;
    f32 yaw;
    f32 pitch;
    f32 aspectRatio;
    f32 fov = 45.f;

    glm::vec3 forwardVector;
    glm::vec3 rightVector;
};

struct ApplicationState
{
    TransientDrawingInfo transientInfo;
    PersistentDrawingInfo persistentInfo;
    CameraInfo cameraInfo;
    bool running;
};

struct Vec3
{
    f32 x;
    f32 y;
    f32 z;
};

internal u64 Win32GetWallClock()
{
    LARGE_INTEGER wallClock;
    QueryPerformanceCounter(&wallClock);
    return wallClock.QuadPart;
}

internal f32 Win32GetWallClockPeriod()
{
    LARGE_INTEGER perfCounterFrequency;
    QueryPerformanceFrequency(&perfCounterFrequency);
    return 1000.f / perfCounterFrequency.QuadPart;
}

internal float Win32GetTime()
{
    return Win32GetWallClock() * Win32GetWallClockPeriod() / 1000.f;
}

internal void DebugPrintA(const char *formatString, ...)
{
    CHAR debugString[1024];
    va_list args;
    va_start(args, formatString);
    vsprintf_s(debugString, formatString, args);
    va_end(args);
    OutputDebugStringA(debugString);
}

struct EnvironmentMap
{
    bool initialized;

    u32 FBOs[6];
    u32 quads[6];

    u32 skyboxTexture;
};

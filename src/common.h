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

#define intMax(x, y) (((x) > (y)) ? (x) : (y))
#define intMin(x, y) (((x) < (y)) ? (x) : (y))
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
#define DIR_SHADOW_MAP_SIZE 4096
#define POINT_SHADOW_MAP_SIZE 1024

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
    glm::vec3 tangent;
    glm::vec3 bitangent;
};

enum class TextureType
{
    Diffuse,
    Specular,
    Normals,
    Displacement
};

struct Texture
{
    u32 id;
    TextureType type;
    u64 hash;
};

struct Material
{
    Texture diffuse;
    Texture specular;
    Texture normals;
    Texture displacement;
};

struct Mesh
{
    Vertex *vertices;
    u32 verticesSize;
    u32 *indices;
    u32 indicesSize;
    Material material;
    u32 numTextures;

    glm::mat4 relativeTransform;
};

#define MAX_MESHES_PER_MODEL 100

struct TextureHandles
{
    u64 diffuseHandle;
    u64 specularHandle;
    u64 normalsHandle;
    u64 displacementHandle;
};

struct TextureHandleBuffer
{
    TextureHandles handleGroups[MAX_MESHES_PER_MODEL];
    u32 numHandleGroups = 0;
};

struct Model
{
    u32 vao;
    u32 commandBuffer;
    u32 meshCount;
    // NOTE: the elements below don't seem like they belong here and reflects a conflation
    // of information about a generic model and information about specific instances of the
    // model that need to be rendered. Once we have the model geometry, we'll probably want
    // to render that same model with different textures, positions and scales.
    // TODO: organize this separation.
    u32 id;
    TextureHandleBuffer textureHandleBuffer;
    glm::vec3 position;
    glm::vec3 scale;
};

#define MAX_CUBES 100

struct Cubes
{
    glm::ivec3 positions[MAX_CUBES];
    TextureHandles textures[MAX_CUBES];
    u32 numCubes = 0;
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

struct Object
{
    u32 id;
    u32 vao;
    // u32 commandBuffer;
    u32 numIndices;
    glm::vec3 position;
    TextureHandles textures = {};
};

#define MAX_OBJECTS 20
#define MAX_MODELS 20

struct ShaderProgram
{
    u32 id = 0;
    char vertexShaderFilename[64];
    FILETIME vertexShaderTime;
    char geometryShaderFilename[64];
    FILETIME geometryShaderTime;
    char fragmentShaderFilename[64];
    FILETIME fragmentShaderTime;

    u32 objectIndices[MAX_OBJECTS];
    u32 numObjects;
    u32 modelIndices[MAX_MODELS];
    u32 numModels;
};

#define MAX_ATTACHMENTS 8

struct Framebuffer
{
    u32 fbo;
    u32 attachments[MAX_ATTACHMENTS];
    u32 rbo;
};

struct Ball
{
    Model *model;
    glm::ivec3 position = glm::ivec3(0, 1, 0);
    glm::ivec3 rotation = glm::ivec3(0, 0, -1);
};

struct TransientDrawingInfo
{
    Object objects[MAX_OBJECTS];
    u32 numObjects;
    Model models[MAX_MODELS];
    u32 numModels;

    ShaderProgram gBufferShader;
    ShaderProgram ssaoShader;
    ShaderProgram ssaoBlurShader;
    ShaderProgram nonPointLightingShader;
    ShaderProgram pointLightingShader;
    ShaderProgram dirDepthMapShader;
    ShaderProgram spotDepthMapShader;
    ShaderProgram pointDepthMapShader;
    ShaderProgram instancedObjectShader;
    ShaderProgram colorShader;
    ShaderProgram outlineShader;
    ShaderProgram glassShader;
    ShaderProgram textureShader;
    ShaderProgram postProcessShader;
    ShaderProgram skyboxShader;
    ShaderProgram geometryShader;
    ShaderProgram gaussianShader;

    Cubes cubes;
    Ball ball;

    u32 matricesUBO;
    u32 textureHandlesUBO;

    u32 cubeVao;
    Model *sphereModel;

    u32 skyboxTexture;

    u32 quadVao;
    Framebuffer mainFramebuffer;
    Framebuffer lightingFramebuffer;
    Framebuffer postProcessingFramebuffer;
    Framebuffer dirShadowMapFramebuffer;
    Framebuffer spotShadowMapFramebuffer;

    u32 pointShadowMapQuad[NUM_POINTLIGHTS];
    u32 pointShadowMapFBO[NUM_POINTLIGHTS];

    Framebuffer gaussianFramebuffers[2];

    u32 ssaoNoiseTexture;
    Framebuffer ssaoFramebuffer;
    Framebuffer ssaoBlurFramebuffer;
};

struct PersistentDrawingInfo
{
    bool initialized;
    bool wireframeMode = false;

    u32 numObjects;
    glm::vec3 objectPositions[MAX_OBJECTS];
    u32 numModels;
    glm::vec3 modelPositions[MAX_MODELS];

    DirLight dirLight;
    PointLight pointLights[NUM_POINTLIGHTS];
    SpotLight spotLight;

    f32 materialShininess = 32.f;
    bool blinn = true;

    f32 gamma = 2.2f;
    f32 exposure = 1.f;
    f32 ssaoSamplingRadius = .5f;
    f32 ssaoPower = 1.f;
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
    bool playing;
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

enum class CWInput
{
    LeftButton
};

struct CWPoint
{
    s32 x;
    s32 y;
};

internal f32 lerp(f32 a, f32 b, f32 alpha)
{
    return a + alpha * (b - a);
}

internal u64 fnv1a(u8 *data, size_t len)
{
    u64 hash = 14695981039346656037;
    u64 fnvPrime = 1099511628211;

    BYTE *currentByte = data;
    BYTE *end = data + len;
    while (currentByte < end)
    {
        hash ^= *currentByte;
        hash *= fnvPrime;
        ++currentByte;
    }

    return hash;
}

internal f32 CreateRandomNumber(f32 min, f32 max)
{
    f32 midpoint = (max + min) / 2.f;
    f32 delta = midpoint - min;
    f32 x = (f32)(rand() % 10);
    x = (rand() > RAND_MAX / 2) ? x : -x;
    return midpoint + delta * (x / 10.f);
}

internal glm::vec3 CreateRandomVec3()
{
    f32 x = (f32)(rand() % 10);
    x = (rand() > RAND_MAX / 2) ? x : -x;
    f32 y = (f32)(rand() % 10);
    y = (rand() > RAND_MAX / 2) ? y : -y;
    f32 z = (f32)(rand() % 10);
    z = (rand() > RAND_MAX / 2) ? z : -z;

    return {x, y, z};
}

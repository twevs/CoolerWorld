#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <wingdi.h>
#include <winuser.h>
#include <windowsx.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h> // sinf().

#include <GL/glew.h>
#include "wglext.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "imgui/imgui.cpp"
#include "imgui/imgui_impl_win32.cpp"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/imgui_demo.cpp"
#include "imgui/imgui_draw.cpp"
#include "imgui/imgui_tables.cpp"
#include "imgui/imgui_widgets.cpp"

#include "assimp/Importer.hpp"
#include <assimp/scene.h>
#include <assimp/postprocess.h>

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

#define myArraySize(arr) \
    (sizeof((arr)) / sizeof((arr[0])))

#define max(x, y) \
    (((x) > (y)) ? (x) : (y))

#define min(x, y) \
    (((x) < (y)) ? (x) : (y))

#define clamp(x, min, max) \
    (((x) < (min)) ? (min) : ((x) > (max)) ? (max) : (x))

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

PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;

global_variable f32 globalFov = 45.f;
global_variable f32 globalAspectRatio;
global_variable glm::vec3 globalCameraPos;
global_variable float globalCameraYaw = 0.f;
global_variable float globalCameraPitch = 0.f;

u64 fnv1a(u8 *data, size_t len)
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

struct Arena
{
    void *memory;
    u64 stackPointer;
    u64 size;
};

global_variable Arena globalArena;

Arena *AllocArena(u64 size)
{
    void *mem = VirtualAlloc(NULL, sizeof(Arena) + size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Arena *newArena = (Arena *)mem;
    newArena->memory = (u8 *)mem + sizeof(Arena);
    newArena->size = size;
    return newArena;
}

void FreeArena(Arena *arena)
{
    VirtualFree(arena, arena->size, MEM_RELEASE);
}

void *ArenaPush(Arena *arena, u64 size)
{
    void *mem = (void *)((u8 *)arena->memory + arena->stackPointer);
    arena->stackPointer += size;
    myAssert(arena->stackPointer <= arena->size);
    return mem;
}

void ArenaPop(Arena *arena, u64 size)
{
    arena->stackPointer -= size;
    myAssert(arena->stackPointer >= 0);
}

void ArenaClear(Arena *arena)
{
    arena->stackPointer = 0;
}

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

Attenuation globalAttenuationTable[] =
{
    { 7   , .7f   , 1.8f    },
    { 13  , .35f  , .44f    },
    { 20  , .22f  , .20f    },
    { 32  , .14f  , .07f    },
    { 50  , .09f  , .032f   },
    { 65  , .07f  , .017f   },
    { 100 , .045f , .0075f  },
    { 160 , .027f , .0028f  },
    { 200 , .022f , .0019f  },
    { 325 , .014f , .0007f  },
    { 600 , .007f , .0002f  },
    { 3250, .0014f, .00007f },
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

struct DrawingInfo
{
    bool initialized;
    bool wireframeMode = false;
    u32 depthTestFunc = GL_LESS;
    
    // Transient data.
    u32 objectShaderProgram;
    u32 lightShaderProgram;
    u32 lightVao;
    
    float clearColor[4] = { .1f, .1f, .1f, 1.f };
    DirLight dirLight;
    PointLight pointLights[NUM_POINTLIGHTS];
    SpotLight spotLight;
    
    Model backpack;
};

void SaveDrawingInfo(DrawingInfo *info)
{
    FILE *file;
    fopen_s(&file, "save.bin", "wb");
    
    fwrite(&globalCameraPos, sizeof(globalCameraPos), 1, file);
    fwrite(&globalCameraPitch, sizeof(globalCameraPitch), 1, file);
    fwrite(&globalCameraYaw, sizeof(globalCameraYaw), 1, file);
    fwrite(&info->wireframeMode, sizeof(info->wireframeMode), 1, file);
    fwrite(&info->depthTestFunc, sizeof(info->depthTestFunc), 1, file);
    fwrite(&info->clearColor, sizeof(info->clearColor), 1, file);
    fwrite(&info->dirLight, sizeof(DirLight), 1, file);
    fwrite(&info->pointLights, sizeof(PointLight), NUM_POINTLIGHTS, file);
    fwrite(&info->spotLight, sizeof(SpotLight), 1, file);
    
    fclose(file);
}

bool LoadDrawingInfo(DrawingInfo *info)
{
    FILE *file;
    errno_t opened = fopen_s(&file, "save.bin", "rb");
    if (opened != 0)
    {
        return false;
    }
    
    fread(&globalCameraPos, sizeof(globalCameraPos), 1, file);
    fread(&globalCameraPitch, sizeof(globalCameraPitch), 1, file);
    fread(&globalCameraYaw, sizeof(globalCameraYaw), 1, file);
    fread(&info->wireframeMode, sizeof(info->wireframeMode), 1, file);
    glPolygonMode(GL_FRONT_AND_BACK, info->wireframeMode ? GL_LINE : GL_FILL);
    fread(&info->depthTestFunc, sizeof(info->depthTestFunc), 1, file);
    glDepthFunc(info->depthTestFunc);
    fread(&info->clearColor, sizeof(info->clearColor), 1, file);
    fread(&info->dirLight, sizeof(DirLight), 1, file);
    fread(&info->pointLights, sizeof(PointLight), NUM_POINTLIGHTS, file);
    fread(&info->spotLight, sizeof(SpotLight), 1, file);
    
    fclose(file);
    
    return true;
}
        
struct VaoInformation
{
    f32 *vertices;
    u32 verticesSize;
    s32 *elemCounts;
    u32 elementCountsSize;
    u32 *indices;
    u32 indicesSize;
};

struct ApplicationState
{
    DrawingInfo drawingInfo;
    bool running;
};

PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;
PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;

LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void DebugPrintA(const char *formatString, ...)
{
    CHAR debugString[1024];
    va_list args;
    va_start(args, formatString);
    vsprintf_s(debugString, formatString, args);
    va_end(args);
    OutputDebugStringA(debugString);
}

void ResizeGLViewport(HWND window)
{
    RECT clientRect;
    GetClientRect(window, &clientRect);
    glViewport(0, 0, clientRect.right, clientRect.bottom);
}

glm::mat4 GetCameraWorldRotation()
{
    // We are rotating in world space so this returns a world-space rotation.
    glm::vec3 cameraYawAxis = glm::vec3(0.f, 1.f, 0.f);
    glm::vec3 cameraPitchAxis = glm::vec3(1.f, 0.f, 0.f);
    
    glm::mat4 cameraYaw = glm::rotate(glm::mat4(1.f), globalCameraYaw, cameraYawAxis);
    glm::mat4 cameraPitch = glm::rotate(glm::mat4(1.f), globalCameraPitch, cameraPitchAxis);
    glm::mat4 cameraRotation = cameraYaw * cameraPitch;
    
    return cameraRotation;
}

glm::vec3 GetCameraRightVector()
{
    // World-space rotation * world-space axis -> world-space value.
    glm::vec4 cameraRightVec = GetCameraWorldRotation() * glm::vec4(1.f, 0.f, 0.f, 0.f);
    
    return glm::vec3(cameraRightVec);
}

glm::vec3 GetCameraForwardVector()
{
    glm::vec4 cameraForwardVec = GetCameraWorldRotation() * glm::vec4(0.f, 0.f, -1.f, 0.f);
    
    return glm::vec3(cameraForwardVec);
}

f32 clampf(f32 x, f32 min, f32 max, f32 safety = 0.f)
{
    return (x < min + safety) ? (min + safety) : (x > max - safety) ? (max - safety) : x;
}

void PrintDepthTestFunc(u32 val, char *outputBuffer, u32 bufSize)
{
    switch (val)
    {
        case GL_NEVER:
            sprintf_s(outputBuffer, bufSize, "GL_NEVER\n");
            break;
        case GL_LESS:
            sprintf_s(outputBuffer, bufSize, "GL_LESS\n");
            break;
        case GL_EQUAL:
            sprintf_s(outputBuffer, bufSize, "GL_EQUAL\n");
            break;
        case GL_LEQUAL:
            sprintf_s(outputBuffer, bufSize, "GL_LEQUAL\n");
            break;
        case GL_GREATER:
            sprintf_s(outputBuffer, bufSize, "GL_GREATER\n");
            break;
        case GL_NOTEQUAL:
            sprintf_s(outputBuffer, bufSize, "GL_NOTEQUAL\n");
            break;
        case GL_GEQUAL:
            sprintf_s(outputBuffer, bufSize, "GL_GEQUAL\n");
            break;
        case GL_ALWAYS:
            sprintf_s(outputBuffer, bufSize, "GL_ALWAYS\n");
            break;
        default:
            myAssert(false);
            break;
    }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void Win32ProcessMessages(
    HWND window,
    bool *running,
    DrawingInfo *drawingInfo,
    glm::vec3 *movement,
    ImGuiIO *imGuiIO)
{
    MSG message;
    while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE))
    {        
        if (ImGui_ImplWin32_WndProcHandler(window, message.message, message.wParam, message.lParam))
        {
            break;
        }
        if (imGuiIO->WantCaptureKeyboard || imGuiIO->WantTextInput)
        {
            TranslateMessage(&message);
            DispatchMessage(&message);
            break;
        }
        local_persist bool capturing = false;
        local_persist f32 speed = .1f;
        
        POINT windowOrigin = {};
        ClientToScreen(window, &windowOrigin);
        s16 originX = (s16)windowOrigin.x;
        s16 originY = (s16)windowOrigin.y;
        
        RECT clientRect;
        GetClientRect(window, &clientRect);
        s16 centreX = originX + (s16)clientRect.right / 2;
        s16 centreY = originY + (s16)clientRect.bottom / 2;
        
        switch (message.message)
        {
        case WM_QUIT:
            *running = false;
            break;
        case WM_RBUTTONDOWN:
            SetCapture(window);
            capturing = true;
            ShowCursor(FALSE);
            SetCursorPos(centreX, centreY);
            break;
        case WM_RBUTTONUP:
            ShowCursor(TRUE);
            ReleaseCapture();
            capturing = false;
            break;
        case WM_MOUSEMOVE: {
            bool rightButtonDown = (message.wParam & 0x2);
            if (capturing)
            {
                // NOTE: we have to use these macros instead of LOWORD() and HIWORD() for things to
                // function correctly on systems with multiple monitors, see:
                // https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-rbuttondown
                s16 xCoord = originX + GET_X_LPARAM(message.lParam);
                s16 yCoord = originY + GET_Y_LPARAM(message.lParam);
                globalCameraYaw -= (xCoord - centreX) / 100.f;
                globalCameraPitch = clampf(globalCameraPitch - (yCoord - centreY) / 100.f,
                         -PI/2,
                         PI/2,
                         .01f);
                    
                SetCursorPos(centreX, centreY);
            }
            break;
        }
        case WM_MOUSEWHEEL: {
            s16 wheelRotation = HIWORD(message.wParam);
            bool shiftPressed = (GetKeyState(VK_SHIFT) < 0);
            if (shiftPressed)
            {
                globalFov = clampf(globalFov - (f32)wheelRotation * 3.f / WHEEL_DELTA, 0.f, 90.f);
            }
            else
            {
                speed = fmax(speed + (f32)wheelRotation / (20.f * WHEEL_DELTA), 0.f);
                DebugPrintA("New speed: %f\n", speed);
            }
            break;
        }
        case WM_CHAR:
            imGuiIO->AddInputCharacter((u32)message.wParam);
            break;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            // Treat WASD specially.
            float deltaZ = (GetKeyState('W') < 0) ? .1f : (GetKeyState('S') < 0) ? -.1f : 0.f;
            float deltaX = (GetKeyState('D') < 0) ? .1f : (GetKeyState('A') < 0) ? -.1f : 0.f;
            *movement += GetCameraForwardVector() * deltaZ * speed;
            *movement += GetCameraRightVector() * deltaX * speed;
                
            // Other keys.
            bool altPressed = (message.lParam >> 29) & 1;
            u32 vkCode = (u32)message.wParam;
            switch (vkCode)
            {
            case VK_F4:
                if (altPressed)
                {
                    *running = false;
                }
                break;
            case VK_ESCAPE:
                *running = false;
                break;
            case VK_UP:
                if (altPressed)
                {
                }
                else
                {
                    drawingInfo->spotLight.innerCutoff += .05f;
                    DebugPrintA("innerCutoff: %f\n", drawingInfo->spotLight.innerCutoff);
                }
                break;
            case VK_DOWN:
                if (altPressed)
                {
                }
                else
                {
                    drawingInfo->spotLight.innerCutoff -= .05f;
                    DebugPrintA("innerCutoff: %f\n", drawingInfo->spotLight.innerCutoff);
                }
                break;
            case VK_LEFT:
                drawingInfo->spotLight.outerCutoff -= .05f;
                DebugPrintA("outerCutoff: %f\n", drawingInfo->spotLight.outerCutoff);
                break;
            case VK_RIGHT:
                drawingInfo->spotLight.outerCutoff += .05f;
                DebugPrintA("outerCutoff: %f\n", drawingInfo->spotLight.outerCutoff);
                break;
            case 'Q':
                globalAspectRatio = fmax(globalAspectRatio - .1f, 0.f);
                break;
            case 'E':
                globalAspectRatio = fmin(globalAspectRatio + .1f, 10.f);
                break;
            case 'X':
                drawingInfo->wireframeMode = !drawingInfo->wireframeMode;
                glPolygonMode(GL_FRONT_AND_BACK, drawingInfo->wireframeMode ? GL_LINE : GL_FILL);
                break;
            case 'U':
                drawingInfo->depthTestFunc = max(drawingInfo->depthTestFunc - 1, 0x200);
                glDepthFunc(drawingInfo->depthTestFunc);
                break;
            case 'I':
                drawingInfo->depthTestFunc = min(drawingInfo->depthTestFunc + 1, 0x207);
                glDepthFunc(drawingInfo->depthTestFunc);
                break;
            case 'O':
                break;
            case 'J':
                break;
            case 'K':
                break;
            case 'L':
                break;
            case 'F':
                {
                    local_persist bool flashLightOn = true;
                    flashLightOn = !flashLightOn;
                    drawingInfo->spotLight.ambient = flashLightOn ? glm::vec3(.1f) : glm::vec3(0.f);
                    drawingInfo->spotLight.diffuse = flashLightOn ? glm::vec3(.5f) : glm::vec3(0.f);
                    drawingInfo->spotLight.specular = flashLightOn ? glm::vec3(1.f) : glm::vec3(0.f);
                    break;
                }
            case 'G':
                break;
            case 'H':
                break;
            }
            break;
        }
        default:
            TranslateMessage(&message);
            DispatchMessage(&message);
        }
    }
}

u64 Win32GetWallClock()
{
    LARGE_INTEGER wallClock;
    QueryPerformanceCounter(&wallClock);
    return wallClock.QuadPart;
}

f32 Win32GetWallClockPeriod()
{
    LARGE_INTEGER perfCounterFrequency;
    QueryPerformanceFrequency(&perfCounterFrequency);
    return 1000.f / perfCounterFrequency.QuadPart;
}

float Win32GetTime()
{
    return Win32GetWallClock() * Win32GetWallClockPeriod() / 1000.f;
}

int InitializeOpenGLExtensions(HINSTANCE hInstance)
{
    WCHAR dummyWindowClassName[] = L"DummyOGLExtensionsWindow";

    WNDCLASSW windowClass = {};
    windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    windowClass.lpfnWndProc = DefWindowProcW;
    windowClass.hInstance = hInstance;
    windowClass.lpszClassName = dummyWindowClassName;

    if (!RegisterClassW(&windowClass))
    {
        return -1;
    }

    HWND dummyWindow = CreateWindowW(dummyWindowClassName,                   // lpClassName,
                                     L"Dummy OpenGL Extensions Window", // lpWindowName,
                                     0,                                 // dwStyle,
                                     CW_USEDEFAULT,                     // x,
                                     CW_USEDEFAULT,                     // y,
                                     CW_USEDEFAULT,                     // nWidth,
                                     CW_USEDEFAULT,                     // nHeight,
                                     0,                                 // hWndParent,
                                     0,                                 // hMenu,
                                     hInstance,                         // hInstance,
                                     0                                  // lpParam
    );

    if (!dummyWindow)
    {
        return -1;
    }

    HDC dummyDC = GetDC(dummyWindow);

    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR), //  size of this pfd
        1,                             // version number
        PFD_DRAW_TO_WINDOW |           // support window
            PFD_SUPPORT_OPENGL |       // support OpenGL
            PFD_DOUBLEBUFFER,          // double buffered
        PFD_TYPE_RGBA,                 // RGBA type
        32,                            // 32-bit color depth
        0,
        0,
        0,
        0,
        0,
        0, // color bits ignored
        0, // no alpha buffer
        0, // shift bit ignored
        0, // no accumulation buffer
        0,
        0,
        0,
        0,              // accum bits ignored
        24,             // 24-bit z-buffer
        8,              // 8-bit stencil buffer
        0,              // no auxiliary buffer
        PFD_MAIN_PLANE, // main layer
        0,              // reserved
        0,
        0,
        0 // layer masks ignored
    };

    int pixelFormat = ChoosePixelFormat(dummyDC, &pfd);
    if (pixelFormat == 0)
    {
        return -1;
    }

    SetPixelFormat(dummyDC, pixelFormat, &pfd);

    HGLRC dummyRenderingContext = wglCreateContext(dummyDC);
    if (!dummyRenderingContext)
    {
        return -1;
    }

    if (!wglMakeCurrent(dummyDC, dummyRenderingContext))
    {
        return -1;
    }

    wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
    wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");

    wglMakeCurrent(dummyDC, 0);
    wglDeleteContext(dummyRenderingContext);
    ReleaseDC(dummyWindow, dummyDC);
    DestroyWindow(dummyWindow);

    return 0;
}

bool CompileShader(u32 *shaderID, GLenum shaderType, const char *shaderFilename)
{

    HANDLE file = CreateFileA(shaderFilename,        // lpFileName,
                              GENERIC_READ,          // dwDesiredAccess,
                              FILE_SHARE_READ,       // dwShareMode,
                              0,                     // lpSecurityAttributes,
                              OPEN_EXISTING,         // dwCreationDisposition,
                              FILE_ATTRIBUTE_NORMAL, // dwFlagsAndAttributes,
                              0                      // hTemplateFile
    );
    u32 fileSize = GetFileSize(file, 0);
    char *fileBuffer = (char *)VirtualAlloc(0, fileSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    DWORD bytesRead;
    ReadFile(file, fileBuffer, fileSize, &bytesRead, 0);
    myAssert(fileSize == bytesRead);

    *shaderID = glCreateShader(shaderType);
    glShaderSource(*shaderID, 1, &fileBuffer, NULL);
    VirtualFree(fileBuffer, fileSize, MEM_RELEASE);
    glCompileShader(*shaderID);

    int success;
    char infoLog[512];
    glGetShaderiv(*shaderID, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(*shaderID, 512, NULL, infoLog);
        DebugPrintA("Shader compilation failed: %s\n", infoLog);
        return false;
    }

    return true;
}

bool CreateShaderProgram(u32 *programID, const char *vertexShaderFilename, const char *fragmentShaderFilename)
{
    u32 vertexShaderID;
    if (!CompileShader(&vertexShaderID, GL_VERTEX_SHADER, vertexShaderFilename))
    {
        return false;
    }

    u32 fragmentShaderID;
    if (!CompileShader(&fragmentShaderID, GL_FRAGMENT_SHADER, fragmentShaderFilename))
    {
        return false;
    }

    *programID = glCreateProgram();
    glAttachShader(*programID, vertexShaderID);
    glAttachShader(*programID, fragmentShaderID);
    glLinkProgram(*programID);

    int success;
    char infoLog[512];
    glGetProgramiv(*programID, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(*programID, 512, NULL, infoLog);
        DebugPrintA("Shader program creation failed: %s\n", infoLog);
        return false;
    }

    glDeleteShader(vertexShaderID);
    glDeleteShader(fragmentShaderID);

    return true;
}

glm::mat4 LookAt(
     glm::vec3 cameraPosition,
     glm::vec3 cameraTarget,
     glm::vec3 cameraUpVector,
     float farPlaneDistance)
{
    glm::mat4 inverseTranslation = glm::mat4(1.f);
    inverseTranslation[3][0] = -cameraPosition.x;
    inverseTranslation[3][1] = -cameraPosition.y;
    inverseTranslation[3][2] = -cameraPosition.z;
    
    glm::vec3 direction = glm::normalize(cameraPosition - cameraTarget);
    glm::vec3 rightVector = glm::normalize(glm::cross(cameraUpVector, direction));
    glm::vec3 upVector = (glm::cross(direction, rightVector));
    
    // TODO: investigate why filling the last column with the negative of cameraPosition does not
    // produce the same effect as the multiplication by the inverse translation.
    glm::mat4 inverseRotation = glm::mat4(1.f);
    inverseRotation[0][0] = rightVector.x;
    inverseRotation[1][0] = rightVector.y;
    inverseRotation[2][0] = rightVector.z;
    inverseRotation[0][1] = upVector.x;
    inverseRotation[1][1] = upVector.y;
    inverseRotation[2][1] = upVector.z;
    inverseRotation[0][2] = direction.x;
    inverseRotation[1][2] = direction.y;
    inverseRotation[2][2] = direction.z;
    
    // TODO: figure out what the deal is with the inverse scaling matrix in "Computer graphics:
    // Principles and practice", p. 306. I'm leaving this unused for now as it breaks rendering.
    glm::mat4 inverseScale =
    {
        1.f / (farPlaneDistance * (tanf(globalFov / globalAspectRatio * PI / 180.f) / 2.f)), 0.f, 0.f, 0.f,
        0.f, 1.f / (farPlaneDistance * (tanf(globalFov * PI / 180.f) / 2.f)), 0.f, 0.f,
        0.f, 0.f, 1.f / farPlaneDistance, 0.f,
        0.f, 0.f, 0.f, 1.f
    };
    
    return inverseRotation * inverseTranslation;
}

void SetShaderUniformSampler(u32 shaderProgram, const char *uniformName, u32 slot)
{
    glUniform1i(glGetUniformLocation(shaderProgram, uniformName), slot);
}

void SetShaderUniformFloat(u32 shaderProgram, const char *uniformName, float value)
{
    glUniform1f(glGetUniformLocation(shaderProgram, uniformName), value);
}

void SetShaderUniformVec3(u32 shaderProgram, const char *uniformName, glm::vec3 vector)
{
    glUniform3fv(glGetUniformLocation(shaderProgram, uniformName), 1, glm::value_ptr(vector));
}

void SetShaderUniformMat3(u32 shaderProgram, const char *uniformName, glm::mat3* matrix)
{
    glUniformMatrix3fv(glGetUniformLocation(shaderProgram, uniformName), 1, GL_FALSE, glm::value_ptr(*matrix));
}

void SetShaderUniformMat4(u32 shaderProgram, const char *uniformName, glm::mat4* matrix)
{
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, uniformName), 1, GL_FALSE, glm::value_ptr(*matrix));
}

void DrawWindow(HWND window, HDC hdc, bool *running, DrawingInfo *drawingInfo)
{
    if (!drawingInfo->initialized)
    {
        return;
    }
    
    float *cc = drawingInfo->clearColor;
    glClearColor(cc[0], cc[1], cc[2], cc[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // The camera target should always be camera pos + local forward vector.
    // We want to be able to rotate the camera, however.
    // How do we obtain the forward vector? Translate world forward vec by camera world rotation matrix.
    glm::vec3 cameraForwardVec = glm::normalize(GetCameraForwardVector());
    
    glm::vec3 cameraTarget = globalCameraPos + cameraForwardVec;
        
    // The camera direction vector points from the camera target to the camera itself, maintaining
    // the OpenGL convention of the Z-axis being positive towards the viewer.
    glm::vec3 cameraDirection = glm::normalize(globalCameraPos - cameraTarget);
    
    glm::vec3 upVector = glm::vec3(0.f, 1.f, 0.f);
    glm::vec3 cameraRightVec = glm::normalize(glm::cross(upVector, cameraDirection));
    glm::vec3 cameraUpVec = glm::normalize(glm::cross(cameraDirection, cameraRightVec));
    
    float farPlaneDistance = 100.f;
    glm::mat4 viewMatrix = LookAt(globalCameraPos, cameraTarget, cameraUpVec, farPlaneDistance);
    
    // Projection matrix: transforms vertices from view space to clip space.
    glm::mat4 projectionMatrix = glm::perspective(
        glm::radians(globalFov), 
        globalAspectRatio, 
        .1f, 
        farPlaneDistance);
    
    // Point lights.
    {
        u32 shaderProgram = drawingInfo->lightShaderProgram;
        glUseProgram(shaderProgram);
        
        SetShaderUniformMat4(shaderProgram, "viewMatrix", &viewMatrix);
        SetShaderUniformMat4(shaderProgram, "projectionMatrix", &projectionMatrix);
        
        glBindVertexArray(drawingInfo->lightVao);
        
        for (u32 lightIndex = 0; lightIndex < NUM_POINTLIGHTS; lightIndex++)
        {
            PointLight *curLight = &drawingInfo->pointLights[lightIndex];
            // Model matrix: transforms vertices from local to world space.
            glm::mat4 modelMatrix = glm::mat4(1.f);
            modelMatrix = glm::translate(modelMatrix, curLight->position);
        
            SetShaderUniformVec3(drawingInfo->lightShaderProgram, "lightColor", curLight->diffuse);
            SetShaderUniformMat4(shaderProgram, "modelMatrix", &modelMatrix);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }
    }
    
    // Object.
    {
        u32 shaderProgram = drawingInfo->objectShaderProgram;
        glUseProgram(shaderProgram);
        
        SetShaderUniformFloat(shaderProgram, "material.shininess", 32.f);
        
        SetShaderUniformVec3(shaderProgram, "dirLight.direction", drawingInfo->dirLight.direction);
        SetShaderUniformVec3(shaderProgram, "dirLight.ambient", drawingInfo->dirLight.ambient);
        SetShaderUniformVec3(shaderProgram, "dirLight.diffuse", drawingInfo->dirLight.diffuse);
        SetShaderUniformVec3(shaderProgram, "dirLight.specular", drawingInfo->dirLight.specular);
        
        for (u32 lightIndex = 0; lightIndex < NUM_POINTLIGHTS; lightIndex++)
        {
            PointLight *lights = drawingInfo->pointLights;
            PointLight light = lights[lightIndex];
            
            char uniformString[32];
            sprintf_s(uniformString, "pointLights[%i].position", lightIndex);
            SetShaderUniformVec3(shaderProgram, uniformString, light.position);
            sprintf_s(uniformString, "pointLights[%i].ambient", lightIndex);
            SetShaderUniformVec3(shaderProgram, uniformString, light.ambient);
            sprintf_s(uniformString, "pointLights[%i].diffuse", lightIndex);
            SetShaderUniformVec3(shaderProgram, uniformString, light.diffuse);
            sprintf_s(uniformString, "pointLights[%i].specular", lightIndex);
            SetShaderUniformVec3(shaderProgram, uniformString, light.specular);
            
            Attenuation *att = &globalAttenuationTable[light.attIndex];
            sprintf_s(uniformString, "pointLights[%i].linear", lightIndex);
            SetShaderUniformFloat(shaderProgram, uniformString, att->linear);
            sprintf_s(uniformString, "pointLights[%i].quadratic", lightIndex);
            SetShaderUniformFloat(shaderProgram, uniformString, att->quadratic);
        }
        
        SetShaderUniformVec3(shaderProgram, "spotLight.position", globalCameraPos);
        SetShaderUniformVec3(shaderProgram, "spotLight.direction", GetCameraForwardVector());
        SetShaderUniformFloat(shaderProgram, "spotLight.innerCutoff", cosf(drawingInfo->spotLight.innerCutoff));
        SetShaderUniformFloat(shaderProgram, "spotLight.outerCutoff", cosf(drawingInfo->spotLight.outerCutoff));
        SetShaderUniformVec3(shaderProgram, "spotLight.ambient", drawingInfo->spotLight.ambient);
        SetShaderUniformVec3(shaderProgram, "spotLight.diffuse", drawingInfo->spotLight.diffuse);
        SetShaderUniformVec3(shaderProgram, "spotLight.specular", drawingInfo->spotLight.specular);
        
        SetShaderUniformVec3(shaderProgram, "cameraPos", globalCameraPos);
        
        Model *model = &drawingInfo->backpack;
        for (u32 i = 0; i < model->meshCount; i++)
        {
            glBindVertexArray(model->vaos[i]);
    
            Mesh *mesh = &drawingInfo->backpack.meshes[i];
            if (mesh->numTextures > 0)
            {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, mesh->textures[0].id);
            }
            if (mesh->numTextures > 1)
            {
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, mesh->textures[1].id);
            }
            
            // Model matrix: transforms vertices from local to world space.
            glm::mat4 modelMatrix = glm::mat4(1.f);
            modelMatrix = glm::translate(modelMatrix, glm::vec3(0.f));
        
            glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(modelMatrix)));

            SetShaderUniformMat4(shaderProgram, "modelMatrix", &modelMatrix);
            SetShaderUniformMat3(shaderProgram, "normalMatrix", &normalMatrix);
            SetShaderUniformMat4(shaderProgram, "viewMatrix", &viewMatrix);
            SetShaderUniformMat4(shaderProgram, "projectionMatrix", &projectionMatrix);
            glDrawElements(GL_TRIANGLES, mesh->verticesSize / sizeof(Vertex), GL_UNSIGNED_INT, 0);
        }
    }
    
    u32 id = 0;
    ImGui::Begin("Debug Window");
    
    char depthTestFuncStr[16];
    PrintDepthTestFunc(drawingInfo->depthTestFunc, depthTestFuncStr, sizeof(depthTestFuncStr));
    ImGui::Text("Depth-test function: %s", depthTestFuncStr);
    
    ImGui::Separator();
    
    ImGui::SliderFloat4("Clear color", drawingInfo->clearColor, 0.f, 1.f);
    
    if (ImGui::CollapsingHeader("Directional light"))
    {
        ImGui::PushID(id++);
        ImGui::SliderFloat3("Direction", glm::value_ptr(drawingInfo->dirLight.direction), -10.f, 10.f);
        ImGui::SliderFloat3("Ambient", glm::value_ptr(drawingInfo->dirLight.ambient), 0.f, 1.f);
        ImGui::SliderFloat3("Diffuse", glm::value_ptr(drawingInfo->dirLight.diffuse), 0.f, 1.f);
        ImGui::SliderFloat3("Specular", glm::value_ptr(drawingInfo->dirLight.specular), 0.f, 1.f);
        ImGui::PopID();
    }
    
    if (ImGui::CollapsingHeader("Point lights"))
    {
        PointLight *lights = drawingInfo->pointLights;
        for (u32 index = 0; index < NUM_POINTLIGHTS; index++)
        {
            ImGui::PushID(id++);
            char treeName[32];
            sprintf_s(treeName, "Light #%i", index);
            if (ImGui::TreeNode(treeName))
            {
                ImGui::SliderFloat3("Position", glm::value_ptr(lights[index].position), -150.f, 150.f);
                ImGui::SliderFloat3("Ambient", glm::value_ptr(lights[index].ambient), 0.f, 1.f);
                ImGui::SliderFloat3("Diffuse", glm::value_ptr(lights[index].diffuse), 0.f, 1.f);
                ImGui::SliderFloat3("Specular", glm::value_ptr(lights[index].specular), 0.f, 1.f);
                ImGui::SliderInt("Attenuation", &lights[index].attIndex, 0, myArraySize(globalAttenuationTable) - 1);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
    
    if (ImGui::CollapsingHeader("Spot light"))
    {
        ImGui::PushID(id++);
        ImGui::SliderFloat3("Position", glm::value_ptr(drawingInfo->spotLight.position), -10.f, 10.f);
        ImGui::SliderFloat3("Direction", glm::value_ptr(drawingInfo->spotLight.direction), -10.f, 10.f);
        ImGui::SliderFloat3("Ambient", glm::value_ptr(drawingInfo->spotLight.ambient), 0.f, 1.f);
        ImGui::SliderFloat3("Diffuse", glm::value_ptr(drawingInfo->spotLight.diffuse), 0.f, 1.f);
        ImGui::SliderFloat3("Specular", glm::value_ptr(drawingInfo->spotLight.specular), 0.f, 1.f);
        ImGui::SliderFloat("Inner cutoff", &drawingInfo->spotLight.innerCutoff, 0.f, PI / 2.f);
        ImGui::SliderFloat("Outer cutoff", &drawingInfo->spotLight.outerCutoff, 0.f, PI / 2.f);
        ImGui::PopID();
    }
    
    ImGui::End();
    
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (!SwapBuffers(hdc))
    {
        if (MessageBoxW(window, L"Failed to swap buffers", L"OpenGL error", MB_OK) == S_OK)
        {
            *running = false;
        }
    }
}

u32 CreateTextureFromImage(const char *filename)
{
    s32 width;
    s32 height;
    s32 numChannels;
    stbi_set_flip_vertically_on_load_thread(true);
    uchar *textureData = stbi_load(filename, &width, &height, &numChannels, 0);
    myAssert(textureData);

    u32 texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    bool alpha = strstr(filename, "png") != NULL;
    GLenum pixelFormat = alpha ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, pixelFormat, GL_UNSIGNED_BYTE, textureData);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(textureData);

    return texture;
}

struct LoadedTextures
{
    Texture *textures;
    u32 numTextures;
};

void LoadTextures(Mesh *mesh, u64 num, aiMaterial *material, aiTextureType type, Arena *texturesArena, LoadedTextures *loadedTextures)
{
    for (u32 i = 0; i < num; i++)
    {
        aiString path;
        material->GetTexture(type, i, &path);
        u64 hash = fnv1a((u8 *)path.C_Str(), strlen(path.C_Str()));
        bool skip = false;
        for (u32 j = 0; j < loadedTextures->numTextures; j++)
        {
            if (loadedTextures->textures[j].hash == hash)
            {
                mesh->textures[mesh->numTextures].id = loadedTextures->textures[j].id;
                mesh->textures[mesh->numTextures].type = loadedTextures->textures[j].type;
                mesh->textures[mesh->numTextures].hash = hash;
                skip = true;
                break;
            }
        }
        if (!skip)
        {
            Texture *texture = (Texture *)ArenaPush(texturesArena, sizeof(Texture));
            texture->id = CreateTextureFromImage(path.C_Str());
            texture->type = (type == aiTextureType_DIFFUSE) ? TextureType::Diffuse : TextureType::Specular;
            texture->hash = hash;
            mesh->textures[mesh->numTextures].id = texture->id;
            mesh->textures[mesh->numTextures].type = texture->type;
            mesh->textures[mesh->numTextures].hash = texture->hash;
            loadedTextures->textures[loadedTextures->numTextures++] = *texture;
        }
        mesh->numTextures++;
    }
}

Mesh ProcessMesh(aiMesh *mesh, const aiScene *scene, Arena *texturesArena, LoadedTextures *loadedTextures)
{
    Mesh result = {};
    
    result.verticesSize = mesh->mNumVertices * sizeof(Vertex);
    result.vertices = (Vertex *)ArenaPush(&globalArena, result.verticesSize);
    myAssert(((u8 *)result.vertices + result.verticesSize) == ((u8 *)globalArena.memory + globalArena.stackPointer));
    
    for (u32 i = 0; i < mesh->mNumVertices; i++)
    {
        result.vertices[i].position.x = mesh->mVertices[i].x;
        result.vertices[i].position.y = mesh->mVertices[i].y;
        result.vertices[i].position.z = mesh->mVertices[i].z;
        
        result.vertices[i].normal.x = mesh->mNormals[i].x;
        result.vertices[i].normal.y = mesh->mNormals[i].y;
        result.vertices[i].normal.z = mesh->mNormals[i].z;
        
        if (mesh->mTextureCoords[0])
        {
            result.vertices[i].texCoords.x = mesh->mTextureCoords[0][i].x;
            result.vertices[i].texCoords.y = mesh->mTextureCoords[0][i].y;
        }
    }
    
    result.indices = (u32 *)((u8 *)result.vertices + result.verticesSize);
    u32 indicesCount = 0;
    for (u32 i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];
        
        u32 *faceIndices = (u32 *)ArenaPush(&globalArena, sizeof(u32) * face.mNumIndices);
        for (u32 j = 0; j < face.mNumIndices; j++)
        {
            faceIndices[j] = face.mIndices[j];
            indicesCount++;
        }
    }
    result.indicesSize = indicesCount * sizeof(u32);
    myAssert(((u8 *)result.indices + result.indicesSize) == ((u8 *)globalArena.memory + globalArena.stackPointer));
    
    if (mesh->mMaterialIndex >= 0)
    {
        aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
        
        u32 numDiffuse = material->GetTextureCount(aiTextureType_DIFFUSE);
        u32 numSpecular = material->GetTextureCount(aiTextureType_SPECULAR);
        u32 numTextures = numDiffuse + numSpecular;
        
        u64 texturesSize = sizeof(Texture) * numTextures;
        result.textures = (Texture *)ArenaPush(&globalArena, texturesSize);
        
        LoadTextures(&result, numDiffuse, material, aiTextureType_DIFFUSE, texturesArena, loadedTextures);
        LoadTextures(&result, numSpecular, material, aiTextureType_SPECULAR, texturesArena, loadedTextures);
    }
    
    return result;
}

void ProcessNode(aiNode *node, const aiScene *scene, Mesh *meshes, u32 *meshCount, Arena *texturesArena, LoadedTextures *loadedTextures)
{
    for (u32 i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
        meshes[*meshCount] = ProcessMesh(mesh, scene, texturesArena, loadedTextures);
        *meshCount += 1;
    }
    
    for (u32 i = 0; i < node->mNumChildren; i++)
    {
        ProcessNode(node->mChildren[i], scene, meshes, meshCount, texturesArena, loadedTextures);
    }
}

u32 CreateVAO(VaoInformation *vaoInfo)
{
    u32 vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // Create and bind VBO.
    u32 vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    // Assume VAO and VBO already bound by this point.
    glBufferData(GL_ARRAY_BUFFER, vaoInfo->verticesSize, vaoInfo->vertices, GL_STATIC_DRAW);

    u32 ebo;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

    glBufferData(GL_ELEMENT_ARRAY_BUFFER, vaoInfo->indicesSize, vaoInfo->indices, GL_STATIC_DRAW);
    
    u32 total = 0;
    for (u32 elemCount = 0; elemCount < vaoInfo->elementCountsSize; elemCount++)
    {
        total += vaoInfo->elemCounts[elemCount];
    }

    u32 accumulator = 0;
    for (u32 index = 0; index < vaoInfo->elementCountsSize; index++)
    {
        u32 elemCount = vaoInfo->elemCounts[index];
        
        glVertexAttribPointer(
            index,
            elemCount,
            GL_FLOAT,
            GL_FALSE,
            total * sizeof(float),
            (void *)(accumulator * sizeof(float)));
        
        accumulator += elemCount;
        
        glEnableVertexAttribArray(index);
    }
    
    return vao;
}

Model LoadModel(const char *filename, s32 *elemCounts, u32 elemCountsSize)
{
    Model result;
    
    Assimp::Importer importer;
    char path[] = "backpack.obj";
    const aiScene *scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);
    myAssert(scene);
    Mesh meshes[2048];
    u32 meshCount = 0;

    Arena *texturesArena = AllocArena(1024);
    LoadedTextures loadedTextures = {};
    loadedTextures.textures = (Texture *)texturesArena->memory;
    ProcessNode(scene->mRootNode, scene, meshes, &meshCount, texturesArena, &loadedTextures);
    FreeArena(texturesArena);

    u32 meshVAOs[2048];
    for (u32 i = 0; i < meshCount; i++)
    {
        Mesh *mesh = &meshes[i];
    
        VaoInformation meshVaoInfo;
        meshVaoInfo.vertices = (f32 *)mesh->vertices;
        meshVaoInfo.verticesSize = mesh->verticesSize;
        meshVaoInfo.elemCounts = elemCounts;
        meshVaoInfo.elementCountsSize = elemCountsSize;
        meshVaoInfo.indices = mesh->indices;
        meshVaoInfo.indicesSize = mesh->indicesSize;

        meshVAOs[i] = CreateVAO(&meshVaoInfo);
    }
    result.meshes = meshes;
    result.meshCount = meshCount;
    result.vaos = meshVAOs;
    
    return result;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    if (InitializeOpenGLExtensions(hInstance) == -1)
    {
        OutputDebugStringW(L"Failed to initialize OpenGL extensions.");
        return -1;
    }

    WCHAR windowClassName[] = L"LearnOpenGLWindowClass";

    WNDCLASSW windowClass = {};
    windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    windowClass.lpfnWndProc = WndProc;
    windowClass.hInstance = hInstance;
    windowClass.hbrBackground = (HBRUSH)(NULL + 1);
    windowClass.lpszClassName = windowClassName;

    RegisterClassW(&windowClass);
    
    ApplicationState appState = {};

    HWND window = CreateWindowW(windowClassName,     // lpClassName,
                                L"Learn OpenGL",     // lpWindowName,
                                WS_OVERLAPPEDWINDOW, // dwStyle,
                                CW_USEDEFAULT,       // x,
                                CW_USEDEFAULT,       // y,
                                CW_USEDEFAULT,       // nWidth,
                                CW_USEDEFAULT,       // nHeight,
                                NULL,                // hWndParent,
                                NULL,                // hMenu,
                                hInstance,           // hInstance,
                                &appState            // lpParam
    );

    if (window)
    {
        ShowWindow(window, nCmdShow);
        UpdateWindow(window);

        HDC hdc = GetDC(window);
        if (!hdc)
        {
            OutputDebugStringW(L"Failed to initialize hardware device context.");
            return -1;
        }

        int pixelFormatAttribs[] = {WGL_DRAW_TO_WINDOW_ARB,
                                    GL_TRUE,
                                    WGL_SUPPORT_OPENGL_ARB,
                                    GL_TRUE,
                                    WGL_DOUBLE_BUFFER_ARB,
                                    GL_TRUE,
                                    WGL_PIXEL_TYPE_ARB,
                                    WGL_TYPE_RGBA_ARB,
                                    WGL_COLOR_BITS_ARB,
                                    32,
                                    WGL_DEPTH_BITS_ARB,
                                    24,
                                    WGL_STENCIL_BITS_ARB,
                                    8,
                                    0};

        int format;
        u32 formats;
        if (!wglChoosePixelFormatARB(hdc, pixelFormatAttribs, NULL, 1, &format, &formats) || formats == 0)
        {
            OutputDebugStringW(L"Failed to choose pixel format.");
            return -1;
        }

        PIXELFORMATDESCRIPTOR pixelFormatDescriptor = {};
        pixelFormatDescriptor.nSize = sizeof(PIXELFORMATDESCRIPTOR);
        if (!DescribePixelFormat(hdc, format, sizeof(PIXELFORMATDESCRIPTOR), &pixelFormatDescriptor))
        {
            OutputDebugStringW(L"Failed to describe pixel format.");
            return -1;
        }

        SetPixelFormat(hdc, format, &pixelFormatDescriptor);

        int contextAttribs[] = {WGL_CONTEXT_MAJOR_VERSION_ARB,
                                3,
                                WGL_CONTEXT_MINOR_VERSION_ARB,
                                3,
                                WGL_CONTEXT_PROFILE_MASK_ARB,
                                WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
#ifndef NDEBUG
                                WGL_CONTEXT_FLAGS_ARB,
                                WGL_CONTEXT_DEBUG_BIT_ARB,
#endif
                                0};

        HGLRC renderingContext = wglCreateContextAttribsARB(hdc, NULL, contextAttribs);
        if (!renderingContext)
        {
            OutputDebugStringW(L"Failed to create OpenGL rendering context.");
            return -1;
        }

        if (!wglMakeCurrent(hdc, renderingContext))
        {
            OutputDebugStringW(L"Failed to make the created OpenGL rendering context the hardware device context's "
                               L"current rendering context.");
            return -1;
        }
        
        wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
        if (!wglSwapIntervalEXT(1))
        {
            OutputDebugStringW(L"Failed to make the created OpenGL rendering context the hardware device context's "
                               L"current rendering context.");
            return -1;
        }

        if (glewInit() != GLEW_OK)
        {
            __debugbreak();
        }

        glEnable(GL_DEPTH_TEST);
#ifndef NDEBUG
        // glDebugMessageCallback(&DebugCallback, NULL);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif

        ResizeGLViewport(window);

        // MessageBoxA(NULL, (char *)glGetString(GL_VERSION), "OpenGL version", MB_OK);

        // Shader initialization.
        
        u32 objectShaderProgram;
        if (!CreateShaderProgram(&objectShaderProgram, "vertex_shader.vs", "fragment_shader.fs"))
        {
            return -1;
        }
        
        u32 lightShaderProgram;
        if (!CreateShaderProgram(&lightShaderProgram, "vertex_shader.vs", "light_fragment_shader.fs"))
        {
            return -1;
        }

        s32 elemCounts[] = { 3, 3, 2 };

        FILE *rectFile;
        fopen_s(&rectFile, "rect.bin", "rb");
        // 4 vertices per face * 6 faces * (vec3 pos + vec3 normal + vec2 texCoords).
        f32 rectVertices[192];
        u32 rectIndices[36];
        fread(rectVertices, sizeof(f32), myArraySize(rectVertices), rectFile);
        fread(rectIndices, sizeof(u32), myArraySize(rectIndices), rectFile);
        fclose(rectFile);
        
        VaoInformation vaoInfo = {};
        vaoInfo.vertices = rectVertices;
        vaoInfo.verticesSize = sizeof(rectVertices);
        vaoInfo.elemCounts = elemCounts;
        vaoInfo.elementCountsSize = myArraySize(elemCounts);
        vaoInfo.indices = rectIndices;
        vaoInfo.indicesSize = sizeof(rectIndices);
        
        u32 lightVao = CreateVAO(&vaoInfo);
        
        DrawingInfo *drawingInfo = &appState.drawingInfo;
        drawingInfo->objectShaderProgram = objectShaderProgram;
        drawingInfo->lightShaderProgram = lightShaderProgram;
        drawingInfo->lightVao = lightVao;
        
        if (!LoadDrawingInfo(drawingInfo))
        {
            PointLight *pointLights = drawingInfo->pointLights;
            
            srand((u32)Win32GetWallClock());
            for (u32 lightIndex = 0; lightIndex < NUM_POINTLIGHTS; lightIndex++)
            {
                f32 x = (f32)(rand() % 10);
                x = (rand() > RAND_MAX / 2) ? x : -x;
                f32 y = (f32)(rand() % 10);
                y = (rand() > RAND_MAX / 2) ? y : -y;
                f32 z = (f32)(rand() % 10);
                z = (rand() > RAND_MAX / 2) ? z : -z;
                pointLights[lightIndex].position = { x, y, z };
            
                u32 attIndex = rand() % myArraySize(globalAttenuationTable);
                // NOTE: constant-range point lights makes for easier visualization of light effects.
                pointLights[lightIndex].attIndex = 4; // clamp(attIndex, 2, 6)
            }
            
            drawingInfo->dirLight.direction =
                glm::normalize(pointLights[NUM_POINTLIGHTS - 1].position - pointLights[0].position);
        }
        
        drawingInfo->backpack = LoadModel("backpack.obj", elemCounts, myArraySize(elemCounts));
        
        drawingInfo->initialized = true;
        
        RECT clientRect;
        GetClientRect(window, &clientRect);
        f32 width = (f32)clientRect.right;
        f32 height = (f32)clientRect.bottom;
        globalAspectRatio = width / height;
        
        glUseProgram(objectShaderProgram);
        SetShaderUniformSampler(objectShaderProgram, "material.diffuse", 0);
        SetShaderUniformSampler(objectShaderProgram, "material.specular", 1);
        
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        ImGui::StyleColorsDark();
        ImGui_ImplWin32_Init(window);
        ImGui_ImplOpenGL3_Init("#version 330");
        
        u64 arenaSize = 100 * 1024 * 1024;
        globalArena = *AllocArena(arenaSize);
        
        glm::vec3 movementPerFrame = {};
        
        f32 targetFrameTime = 1000.f / 60;
        f32 deltaTime = targetFrameTime;

        u64 lastFrameCount = Win32GetWallClock();

        appState.running = true;
        while (appState.running)
        {    
            Win32ProcessMessages(window, &appState.running, &appState.drawingInfo, &movementPerFrame, &io);
            
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            
            u64 currentFrameCount = Win32GetWallClock();
            u64 diff = currentFrameCount - lastFrameCount;
            lastFrameCount = currentFrameCount;

            deltaTime = (f32)diff * Win32GetWallClockPeriod();
            // DebugPrintA("deltaTime: %f\n", deltaTime);

            // Handle resuming from a breakpoint.
            if (deltaTime >= 1000.f)
            {
                deltaTime = targetFrameTime;
            }

            // Game loop uses deltaTime value here ...

            // TODO: investigate whether it's worth keeping this around as a framerate cap mechanism.
            // Won't it produce screen-tearing since it won't necessarily be synchronized with the
            // v-blank? How many users actually disable v-sync anyway?
        
            // WCHAR frameTimeString[32];
            // swprintf_s(frameTimeString, L"Frame time: %f ms\n", frameTimeAccumulator);
            // OutputDebugStringW(frameTimeString);
        
            globalCameraPos += movementPerFrame * deltaTime;
            DrawWindow(window, hdc, &appState.running, &appState.drawingInfo);
            movementPerFrame = glm::vec3(0.f);
            
            // DebugPrintA("Camera pitch: %f\n", globalCameraPitch);
            // DebugPrintA("Camera yaw: %f\n", globalCameraYaw);
        }
    }
    
    SaveDrawingInfo(&appState.drawingInfo);

    return 0;
}

LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    local_persist ApplicationState *appState = NULL;
    
    switch (uMsg)
    {
    case WM_CREATE: {
        CREATESTRUCT *createStruct = (CREATESTRUCT *)lParam;
        appState = (ApplicationState *)createStruct->lpCreateParams;
        return 0;
    }
    case WM_CLOSE:
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT paintStruct;
        BeginPaint(hWnd, &paintStruct);
        EndPaint(hWnd, &paintStruct);
        return 0;
    }
    case WM_SIZE: {
        ResizeGLViewport(hWnd);
        HDC hdc = GetDC(hWnd);
        return 0;
    }
    case WM_ERASEBKGND:
        // See: https://stackoverflow.com/questions/43670470/drawn-opengl-disappearing-on-window-resize
        return 1;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

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

global_variable f32 globalFov = 45.f;
global_variable f32 globalAspectRatio;
global_variable glm::vec3 globalCameraPos;
global_variable float globalCameraYaw = 0.f;
global_variable float globalCameraPitch = 0.f;

struct DrawingInfo
{
    bool initialized;
    
    float mixAlpha;
    u32 texture1;
    u32 texture2;
    
    u32 containerShaderProgram;
    u32 lightShaderProgram;
    
    glm::vec3 containerPos;
    u32 containerVao;
    
    glm::vec3 lightPos;
    u32 lightVao;
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

void Win32ProcessMessages(
    HWND window,
    bool *running,
    bool *wireframeMode,
    DrawingInfo *drawingInfo)
{
    MSG message;
    while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE))
    {
        local_persist bool capturing = false;
        
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
            return;
        case WM_RBUTTONDOWN:
            SetCapture(window);
            capturing = true;
            ShowCursor(FALSE);
            SetCursorPos(centreX, centreY);
            return;
        case WM_RBUTTONUP:
            ShowCursor(TRUE);
            ReleaseCapture();
            capturing = false;
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
            return;
        }
        case WM_MOUSEWHEEL: {
            s16 wheelRotation = HIWORD(message.wParam);
            globalFov = clampf(globalFov - (f32)wheelRotation * 3.f / WHEEL_DELTA, 0.f, 90.f);
            return;
        }
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            // Treat WASD specially.
            float deltaZ = (GetKeyState('W') < 0) ? .1f : (GetKeyState('S') < 0) ? -.1f : 0.f;
            float deltaX = (GetKeyState('D') < 0) ? .1f : (GetKeyState('A') < 0) ? -.1f : 0.f;
            globalCameraPos += GetCameraForwardVector() * deltaZ;
            globalCameraPos += GetCameraRightVector() * deltaX;
                
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
                return;
            case VK_ESCAPE:
                *running = false;
                return;
            case VK_UP:
                drawingInfo->lightPos.y += .1f;
                return;
            case VK_DOWN:
                drawingInfo->lightPos.y -= .1f;
                return;
            case VK_LEFT:
                {
                    glm::vec3 lightToContainer = glm::normalize(drawingInfo->containerPos - drawingInfo->lightPos);
                    drawingInfo->lightPos -= lightToContainer * .1f;
                }
                return;
            case VK_RIGHT:
                {
                    glm::vec3 lightToContainer = glm::normalize(drawingInfo->containerPos - drawingInfo->lightPos);
                    drawingInfo->lightPos += lightToContainer * .1f;
                }
                return;
            case 'Q':
                globalAspectRatio = fmax(globalAspectRatio - .1f, 0.f);
                return;
            case 'E':
                globalAspectRatio = fmin(globalAspectRatio + .1f, 10.f);
                return;
            case 'X':
                *wireframeMode = !*wireframeMode;
                glPolygonMode(GL_FRONT_AND_BACK, *wireframeMode ? GL_LINE : GL_FILL);
                return;
            }
            return;
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

u32 CreateTextureFromImage(const char *filename, bool alpha, GLenum wrapMode)
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

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    GLenum pixelFormat = alpha ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, pixelFormat, GL_UNSIGNED_BYTE, textureData);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(textureData);

    return texture;
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
    
    glClearColor(.1f, .1f, .1f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // glUniform1f(glGetUniformLocation(drawingInfo->shaderProgram, "mixAlpha"), drawingInfo->mixAlpha);
    // glActiveTexture(GL_TEXTURE0);
    // glBindTexture(GL_TEXTURE_2D, drawingInfo->texture1);
    // glActiveTexture(GL_TEXTURE1);
    // glBindTexture(GL_TEXTURE_2D, drawingInfo->texture2);
    
    // The camera target should always be camera pos + local forward vector.
    // We want to be able to rotate the camera, however.
    // How do we obtain the forward vector? Translate world forward vec by camera world rotation matrix.
    glm::vec3 cameraForwardVec = glm::normalize(GetCameraForwardVector());
    
    // float fvX = cosf(PI/2 + globalCameraYaw);
    // float fvY = sinf(globalCameraPitch);
    // float fvZ = -sinf(PI/2 + globalCameraYaw);
    // glm::vec3 cameraForwardVec = glm::normalize(glm::vec3(fvX, fvY, fvZ));
    
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
    
    f32 radius = 5.f;
    // drawingInfo->lightPos.x = radius * cosf(Win32GetWallClock() * Win32GetWallClockPeriod() / 1000.f);
    // drawingInfo->lightPos.z = radius * sinf(Win32GetWallClock() * Win32GetWallClockPeriod() / 1000.f);
    
    glm::vec3 lightColor = glm::vec3(1.f);
    
    // Container.
    {
        u32 shaderProgram = drawingInfo->containerShaderProgram;
        glUseProgram(shaderProgram);
        
        f32 objectAmbient[] = { 0.f, .1f, .06f };
        f32 objectDiffuse[] = { 0.f, .50980392f, .50980392f };
        f32 objectSpecular[] = { .50196078f, .50196078f, .50196078f };
        f32 objectShininess = .25f;
        glUniform3fv(glGetUniformLocation(shaderProgram, "material.ambient"), 1, objectAmbient);
        glUniform3fv(glGetUniformLocation(shaderProgram, "material.diffuse"), 1, objectDiffuse);
        glUniform3fv(glGetUniformLocation(shaderProgram, "material.specular"), 1, objectSpecular);
        SetShaderUniformFloat(shaderProgram, "material.shininess", objectShininess * 128.f);
        
        SetShaderUniformVec3(shaderProgram, "light.position", drawingInfo->lightPos);
        glm::vec3 lightDiffuse = glm::vec3(1.f);
        glm::vec3 lightAmbient = glm::vec3(1.f);
        glm::vec3 lightSpecular = glm::vec3(1.f);
        SetShaderUniformVec3(shaderProgram, "light.ambient", lightAmbient);
        SetShaderUniformVec3(shaderProgram, "light.diffuse", lightDiffuse);
        SetShaderUniformVec3(shaderProgram, "light.specular", lightSpecular);
        
        SetShaderUniformVec3(shaderProgram, "cameraPos", globalCameraPos);
        
        glBindVertexArray(drawingInfo->containerVao);
    
        // Model matrix: transforms vertices from local to world space.
        glm::mat4 modelMatrix = glm::mat4(1.f);
        modelMatrix = glm::translate(modelMatrix, drawingInfo->containerPos);
        
        glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(modelMatrix)));

        SetShaderUniformMat4(shaderProgram, "modelMatrix", &modelMatrix);
        SetShaderUniformMat3(shaderProgram, "normalMatrix", &normalMatrix);
        SetShaderUniformMat4(shaderProgram, "viewMatrix", &viewMatrix);
        SetShaderUniformMat4(shaderProgram, "projectionMatrix", &projectionMatrix);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    }
    
    // Light.
    {
        u32 shaderProgram = drawingInfo->lightShaderProgram;
        glUseProgram(shaderProgram);
        
        SetShaderUniformVec3(drawingInfo->lightShaderProgram, "lightColor", lightColor);
        
        glBindVertexArray(drawingInfo->lightVao);
        
        // Model matrix: transforms vertices from local to world space.
        glm::mat4 modelMatrix = glm::mat4(1.f);
        modelMatrix = glm::translate(modelMatrix, drawingInfo->lightPos);
        
        SetShaderUniformMat4(shaderProgram, "modelMatrix", &modelMatrix);
        SetShaderUniformMat4(shaderProgram, "viewMatrix", &viewMatrix);
        SetShaderUniformMat4(shaderProgram, "projectionMatrix", &projectionMatrix);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    }

    if (!SwapBuffers(hdc))
    {
        if (MessageBoxW(window, L"Failed to swap buffers", L"OpenGL error", MB_OK) == S_OK)
        {
            *running = false;
        }
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

        f32 targetFrameTime = 1000.f / 60;
        f32 deltaTime = targetFrameTime;
        f32 frameTimeAccumulator = 0.f;

        u64 lastFrameCount = Win32GetWallClock();

        // Shader initialization.
        
        u32 containerShaderProgram;
        if (!CreateShaderProgram(&containerShaderProgram, "vertex_shader.vs", "fragment_shader.fs"))
        {
            return -1;
        }
        u32 lightShaderProgram;
        if (!CreateShaderProgram(&lightShaderProgram, "vertex_shader.vs", "light_fragment_shader.fs"))
        {
            return -1;
        }

        u32 texture1 = CreateTextureFromImage("container.jpg", false, GL_REPEAT);
        u32 texture2 = CreateTextureFromImage("awesomeface.png", true, GL_REPEAT);
        
        f32 rectVertices[] = {
            // Positions.    // Normals.
            
            // Face 1: front.
            .5f,  .5f,  .5f, 0.f, 0.f, 1.f, // 0: top right.
            .5f,  -.5f, .5f, 0.f, 0.f, 1.f, // 1: bottom right.
            -.5f, -.5f, .5f, 0.f, 0.f, 1.f, // 2: bottom left.
            -.5f, .5f,  .5f, 0.f, 0.f, 1.f, // 3: top left.
            
            // Face 2: our right.
            .5f,  .5f,  -.5f, 1.f, 0.f, 0.f, // 0: top right.
            .5f,  -.5f, -.5f, 1.f, 0.f, 0.f, // 1: bottom right.
            .5f, -.5f, .5f,   1.f, 0.f, 0.f,  // 2: bottom left.
            .5f, .5f,  .5f,   1.f, 0.f, 0.f,  // 3: top left.
            
            // Face 3: back.
            -.5f,  .5f,  -.5f, 0.f, 0.f, -1.f, // 0: top right.
            -.5f,  -.5f, -.5f, 0.f, 0.f, -1.f, // 1: bottom right.
            .5f, -.5f, -.5f,   0.f, 0.f, -1.f, // 2: bottom left.
            .5f, .5f,  -.5f,   0.f, 0.f, -1.f, // 3: top left.
            
            // Face 4: our left.
            -.5f,  .5f,  .5f, -1.f, 0.f, 0.f,  // 0: top right.
            -.5f,  -.5f, .5f, -1.f, 0.f, 0.f,  // 1: bottom right.
            -.5f, -.5f, -.5f, -1.f, 0.f, 0.f, // 2: bottom left.
            -.5f, .5f,  -.5f, -1.f, 0.f, 0.f, // 3: top left.
            
            // Face 5: top.
            .5f,  .5f,  -.5f, 0.f, 1.f, 0.f, // 0: top right.
            .5f,  .5f, .5f,   0.f, 1.f, 0.f,  // 1: bottom right.
            -.5f, .5f, .5f,   0.f, 1.f, 0.f,  // 2: bottom left.
            -.5f, .5f,  -.5f, 0.f, 1.f, 0.f, // 3: top left.
            
            // Face 6: bottom.
            .5f,  -.5f,  .5f, 0.f, -1.f, 0.f,  // 0: top right.
            .5f,  -.5f, -.5f, 0.f, -1.f, 0.f, // 1: bottom right.
            -.5f, -.5f, -.5f, 0.f, -1.f, 0.f, // 2: bottom left.
            -.5f, -.5f,  .5f, 0.f, -1.f, 0.f,  // 3: top left.
        };
        
        s32 elemCounts[] = { 3, 3 };

        u32 rectIndices[] = {
            0, 1, 3, // Triangle 1.
            1, 2, 3, // Triangle 2.
            
            4, 5, 7, // Triangle 3.
            5, 6, 7, // Triangle 4.
            
            8, 9, 11, // Triangle 5.
            9, 10, 11, // Triangle 6.
            
            12, 13, 15, // Triangle 7.
            13, 14, 15, // Triangle 8.
            
            16, 17, 19, // Triangle 9.
            17, 18, 19, // Triangle 10.
            
            20, 21, 23, // Triangle 11.
            21, 22, 23  // Triangle 12.
        };
        
        VaoInformation vaoInfo = {};
        vaoInfo.vertices = rectVertices;
        vaoInfo.verticesSize = sizeof(rectVertices);
        vaoInfo.elemCounts = elemCounts;
        vaoInfo.elementCountsSize = 2;
        vaoInfo.indices = rectIndices;
        vaoInfo.indicesSize = sizeof(rectIndices);
        
        u32 containerVao = CreateVAO(&vaoInfo);
        u32 lightVao = CreateVAO(&vaoInfo);
        
        bool wireframeMode = false;
        
        RECT clientRect;
        GetClientRect(window, &clientRect);
        f32 width = (f32)clientRect.right;
        f32 height = (f32)clientRect.bottom;
        globalAspectRatio = width / height;
        
        appState.drawingInfo.mixAlpha = .5f;
        appState.drawingInfo.texture1 = texture1;
        appState.drawingInfo.texture2 = texture2;
        
        appState.drawingInfo.containerShaderProgram = containerShaderProgram;
        appState.drawingInfo.lightShaderProgram = lightShaderProgram;
        appState.drawingInfo.containerPos = { 0.f, 0.f, 0.f };
        appState.drawingInfo.containerVao = containerVao;
        // NOTE: X and Y values are set in the drawing function so that the light rotates over time.
        appState.drawingInfo.lightPos = { 2.f, 2.f, 2.f };
        appState.drawingInfo.lightVao = lightVao;
        appState.drawingInfo.initialized = true;
        
        appState.running = true;
        while (appState.running)
        {
            Win32ProcessMessages(window, &appState.running, &wireframeMode, &appState.drawingInfo);

            u64 currentFrameCount = Win32GetWallClock();
            u64 diff = currentFrameCount - lastFrameCount;
            lastFrameCount = currentFrameCount;

            deltaTime = (f32)diff * Win32GetWallClockPeriod();

            // Handle resuming from a breakpoint.
            if (deltaTime >= 1000.f)
            {
                deltaTime = targetFrameTime;
            }

            // Game loop uses deltaTime value here ...

            // TODO: investigate whether it's worth keeping this around as a framerate cap mechanism.
            // Won't it produce screen-tearing since it won't necessarily be synchronized with the
            // v-blank? How many users actually disable v-sync anyway?
            if (frameTimeAccumulator >= targetFrameTime)
            {
                // WCHAR frameTimeString[32];
                // swprintf_s(frameTimeString, L"Frame time: %f ms\n", frameTimeAccumulator);
                // OutputDebugStringW(frameTimeString);

                frameTimeAccumulator = 0.f;

                DrawWindow(window, hdc, &appState.running, &appState.drawingInfo);
                
                // DebugPrintA("Camera pitch: %f\n", globalCameraPitch);
                // DebugPrintA("Camera yaw: %f\n", globalCameraYaw);
            }
            else
            {
                frameTimeAccumulator += deltaTime;
            }
        }
    }

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
        DrawWindow(hWnd, hdc, &appState->running, &appState->drawingInfo);
        return 0;
    }
    case WM_ERASEBKGND:
        // See: https://stackoverflow.com/questions/43670470/drawn-opengl-disappearing-on-window-resize
        return 1;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

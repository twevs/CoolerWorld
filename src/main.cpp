#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <wingdi.h>
#include <winuser.h>
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

#define NUM_CUBES 10
global_variable f32 globalFov = 45.f;
global_variable f32 globalAspectRatio;
global_variable glm::vec3 globalViewTranslation = {0.f, 0.f, -3.f};

struct DrawingInfo
{
    bool initialized;
    u32 shaderProgram;
    float mixAlpha;
    u32 texture1;
    u32 texture2;
    u32 vao;
    glm::vec3 translations[NUM_CUBES];
    f32 rotationDivisors[NUM_CUBES];
    glm::vec3 rotationAxes[NUM_CUBES];
};

struct ApplicationState
{
    DrawingInfo drawingInfo;
    bool running;
};

PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;
PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;

LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void ResizeGLViewport(HWND window)
{
    RECT clientRect;
    GetClientRect(window, &clientRect);
    glViewport(0, 0, clientRect.right, clientRect.bottom);
}

void Win32ProcessMessages(
    HWND window,
    bool *running,
    bool *wireframeMode,
    float *mixAlpha)
{
    MSG message;
    while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE))
    {
        switch (message.message)
        {
        case WM_QUIT:
            *running = false;
            return;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            u32 vkCode = (u32)message.wParam;
            bool altPressed = (message.lParam >> 29) & 1;
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
                *mixAlpha = fmin(*mixAlpha + .025f, 1.f);
                return;
            case VK_DOWN:
                *mixAlpha = fmax(*mixAlpha - .025f, 0.f);
                return;
            case VK_LEFT:
                globalFov = fmax(globalFov - 1.f, 0.f);
                return;
            case VK_RIGHT:
                globalFov = fmin(globalFov + 1.f, 90.f);
                return;
            case 'W':
                if (altPressed)
                {
                    globalViewTranslation.y = fmax(globalViewTranslation.y - .1f, -10.f);
                }
                else
                {
                    globalViewTranslation.z = fmin(globalViewTranslation.z + .1f, .1f);
                }
                return;
            case 'A':
                globalViewTranslation.x = fmin(globalViewTranslation.x + .1f, 10.f);
                return;
            case 'S':
                if (altPressed)
                {
                    globalViewTranslation.y = fmin(globalViewTranslation.y + .1f, 10.f);
                }
                else
                {
                    globalViewTranslation.z = fmax(globalViewTranslation.z - .1f, -100.f);
                }
                return;
            case 'D':
                globalViewTranslation.x = fmax(globalViewTranslation.x - .1f, -10.f);
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

void DebugPrintA(const char *formatString, ...)
{
    CHAR debugString[1024];
    va_list args;
    va_start(args, formatString);
    vsprintf_s(debugString, formatString, args);
    va_end(args);
    OutputDebugStringA(debugString);
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

f32 Win32GetWallClockPeriod()
{
    LARGE_INTEGER perfCounterFrequency;
    QueryPerformanceFrequency(&perfCounterFrequency);
    return 1000.f / perfCounterFrequency.QuadPart;
}

void DrawWindow(HWND window, HDC hdc, bool *running, DrawingInfo *drawingInfo)
{
    if (!drawingInfo->initialized)
    {
        return;
    }
    
    glClearColor(.2f, .3f, .3f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUniform1f(glGetUniformLocation(drawingInfo->shaderProgram, "mixAlpha"), drawingInfo->mixAlpha);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, drawingInfo->texture1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, drawingInfo->texture2);
    
    glBindVertexArray(drawingInfo->vao);
    
    f32 timeValue = (f32)(Win32GetWallClock() * Win32GetWallClockPeriod());
        
    // View matrix: transforms vertices from world to view space.
    // Here, since we want to view the scene from a slight distance, we transform the vertices
    // by "pushing them back" (ie, translating them into -Z).
    glm::mat4 viewMatrix = glm::mat4(1.f);
    viewMatrix = glm::translate(viewMatrix, globalViewTranslation);
    
    // Projection matrix: transforms vertices from view space to clip space.
    glm::mat4 projectionMatrix = glm::perspective(glm::radians(globalFov), globalAspectRatio, .1f, 100.f);
    
    for (u32 cubeIndex = 0; cubeIndex < NUM_CUBES; cubeIndex++)
    {
        // Local transform.
        glm::mat4 localTransform = glm::mat4(1.f);
        localTransform = glm::rotate(
            localTransform,
            glm::radians(timeValue / drawingInfo->rotationDivisors[cubeIndex]),
            drawingInfo->rotationAxes[cubeIndex]
        );
        
        // Model matrix: transforms vertices from local to world space.
        glm::mat4 modelMatrix = glm::mat4(1.f);
        modelMatrix = glm::translate(modelMatrix, drawingInfo->translations[cubeIndex]);
    
        s32 localTransformLoc = glGetUniformLocation(drawingInfo->shaderProgram, "localTransform");
        s32 modelMatrixLoc = glGetUniformLocation(drawingInfo->shaderProgram, "modelMatrix");
        s32 viewMatrixLoc = glGetUniformLocation(drawingInfo->shaderProgram, "viewMatrix");
        s32 projectionMatrixLoc = glGetUniformLocation(drawingInfo->shaderProgram, "projectionMatrix");

        glUniformMatrix4fv(localTransformLoc, 1, GL_FALSE, glm::value_ptr(localTransform));
        glUniformMatrix4fv(modelMatrixLoc, 1, GL_FALSE, glm::value_ptr(modelMatrix));
        glUniformMatrix4fv(viewMatrixLoc, 1, GL_FALSE, glm::value_ptr(viewMatrix));
        glUniformMatrix4fv(projectionMatrixLoc, 1, GL_FALSE, glm::value_ptr(projectionMatrix));
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    }
    
    /*
    glm::mat4 trans2 = glm::mat4(1.f);
    trans2 = glm::translate(trans2, glm::vec3(-.5f, .5f, 0.f));
    trans2 = glm::scale(trans2, glm::vec3(sinf(timeValue / 360.f)));
    glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(trans2));
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    */

    if (!SwapBuffers(hdc))
    {
        if (MessageBoxW(window, L"Failed to swap buffers", L"OpenGL error", MB_OK) == S_OK)
        {
            *running = false;
        }
    }
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
        
        u32 shaderProgram;
        if (!CreateShaderProgram(&shaderProgram, "vertex_shader.vs", "fragment_shader.fs"))
        {
            return -1;
        }

        u32 texture1 = CreateTextureFromImage("container.jpg", false, GL_REPEAT);
        u32 texture2 = CreateTextureFromImage("awesomeface.png", true, GL_REPEAT);

        // Create and bind VAO.
        u32 vao;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        // Create and bind VBO.
        u32 vbo;
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        f32 rectVertices[] = {
            // Positions.    // Texture coords.
            
            // Face 1: front.
            .5f,  .5f,  .5f, 1.f, 1.f, // 0: top right.
            .5f,  -.5f, .5f, 1.f, 0.f, // 1: bottom right.
            -.5f, -.5f, .5f, .0f, .0f, // 2: bottom left.
            -.5f, .5f,  .5f, 0.f, 1.f, // 3: top left.
            
            // Face 2: our right.
            .5f,  .5f,  -.5f, 1.f, 1.f, // 0: top right.
            .5f,  -.5f, -.5f, 1.f, 0.f, // 1: bottom right.
            .5f, -.5f, .5f, .0f, .0f,  // 2: bottom left.
            .5f, .5f,  .5f, 0.f, 1.f,  // 3: top left.
            
            // Face 3: back.
            -.5f,  .5f,  -.5f, 1.f, 1.f, // 0: top right.
            -.5f,  -.5f, -.5f, 1.f, 0.f, // 1: bottom right.
            .5f, -.5f, -.5f, .0f, .0f, // 2: bottom left.
            .5f, .5f,  -.5f, 0.f, 1.f, // 3: top left.
            
            // Face 4: our left.
            -.5f,  .5f,  .5f, 1.f, 1.f,  // 0: top right.
            -.5f,  -.5f, .5f, 1.f, 0.f,  // 1: bottom right.
            -.5f, -.5f, -.5f, .0f, .0f, // 2: bottom left.
            -.5f, .5f,  -.5f, 0.f, 1.f, // 3: top left.
            
            // Face 5: top.
            .5f,  .5f,  -.5f, 1.f, 1.f, // 0: top right.
            .5f,  .5f, .5f, 1.f, 0.f,  // 1: bottom right.
            -.5f, .5f, .5f, .0f, .0f,  // 2: bottom left.
            -.5f, .5f,  -.5f, 0.f, 1.f, // 3: top left.
            
            // Face 6: bottom.
            .5f,  -.5f,  .5f, 1.f, 1.f,  // 0: top right.
            .5f,  -.5f, -.5f, 1.f, 0.f, // 1: bottom right.
            -.5f, -.5f, -.5f, .0f, .0f, // 2: bottom left.
            -.5f, -.5f,  .5f, 0.f, 1.f,  // 3: top left.
        };

        // Assume VAO and VBO already bound by this point.
        glBufferData(GL_ARRAY_BUFFER, sizeof(rectVertices), rectVertices, GL_STATIC_DRAW);

        u32 ebo;
        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

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

        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(rectIndices), rectIndices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);

        glUseProgram(shaderProgram);
        glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);
        glUniform1i(glGetUniformLocation(shaderProgram, "texture2"), 1);
        
        bool wireframeMode = false;
        
        RECT clientRect;
        GetClientRect(window, &clientRect);
        f32 width = (f32)clientRect.right;
        f32 height = (f32)clientRect.bottom;
        globalAspectRatio = width / height;
        
        appState.drawingInfo.shaderProgram = shaderProgram;
        appState.drawingInfo.mixAlpha = .5f;
        appState.drawingInfo.texture1 = texture1;
        appState.drawingInfo.texture2 = texture2;
        appState.drawingInfo.vao = vao;
        
        srand((u32)Win32GetWallClock());
        for (u32 cubeIndex = 0; cubeIndex < NUM_CUBES; cubeIndex++)
        {
            f32 divisor = fmax(5.f, fmod((f32)rand(), 10.f));
            divisor = (rand() > RAND_MAX / 2) ? divisor : -divisor;
            appState.drawingInfo.rotationDivisors[cubeIndex] = (f32)divisor;
            
            f32 x = (f32)rand();
            x /= fmax(x, (f32)rand());
            x = (rand() > RAND_MAX / 2) ? x : -x;
            f32 y = (f32)rand();
            y /= fmax(y, (f32)rand());
            y = (rand() > RAND_MAX / 2) ? y : -y;
            f32 z = (f32)rand();
            z /= fmax(z, (f32)rand());
            z = (rand() > RAND_MAX / 2) ? z : -z;
            appState.drawingInfo.translations[cubeIndex] = glm::vec3(
                 x * pow(cubeIndex, 1.05f),
                 y * pow(cubeIndex, 1.03f),
                 -fabs(z * pow(cubeIndex + 2, 1.3f)));
            appState.drawingInfo.rotationAxes[cubeIndex] = glm::vec3(x, y, z);
        }
        appState.drawingInfo.initialized = true;
        
        appState.running = true;
        while (appState.running)
        {
            Win32ProcessMessages(window, &appState.running, &wireframeMode, &appState.drawingInfo.mixAlpha);

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
                WCHAR frameTimeString[32];
                swprintf_s(frameTimeString, L"Frame time: %f ms\n", frameTimeAccumulator);
                OutputDebugStringW(frameTimeString);

                frameTimeAccumulator = 0.f;

                DrawWindow(window, hdc, &appState.running, &appState.drawingInfo);
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

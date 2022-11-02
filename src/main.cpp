#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <wingdi.h>
#include <winuser.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h> // sinf().

#include <GL/Gl.h>
#include "glcorearb.h"
#include "wglext.h"

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

const char *vertexShaderSource = "#version 330 core\n"
                                 "layout (location = 0) in vec3 aPos;\n"
                                 "layout (location = 1) in vec3 aColor;\n"
                                 "\n"
                                 "out vec3 ourPosition;\n"
                                 "\n"
                                 "void main()\n"
                                 "{\n"
                                 "    gl_Position = vec4(aPos, 1.f);\n"
                                 "    ourPosition = gl_Position.xyz;\n"
                                 "}\0";
const char *fragmentShaderSource = "#version 330 core\n"
                                   "in vec3 ourPosition;\n"
                                   "out vec4 fragColor;\n"
                                   "void main()\n"
                                   "{\n"
                                   "    fragColor = vec4(ourPosition, 1.f);\n"
                                   "}\0";
const char *fragmentShader2Source = "#version 330 core\n"
                                    "out vec4 fragColor;\n"
                                    "void main()\n"
                                    "{\n"
                                    "    fragColor = vec4(1.f, 1.f, 0.f, 1.f);\n"
                                    "}\0";

PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;
PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;
PFNGLGENBUFFERSPROC glGenBuffers;
PFNGLBINDBUFFERPROC glBindBuffer;
PFNGLBUFFERDATAPROC glBufferData;
PFNGLCREATESHADERPROC glCreateShader;
PFNGLSHADERSOURCEPROC glShaderSource;
PFNGLCOMPILESHADERPROC glCompileShader;
PFNGLGETSHADERIVPROC glGetShaderiv;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
PFNGLCREATEPROGRAMPROC glCreateProgram;
PFNGLATTACHSHADERPROC glAttachShader;
PFNGLLINKPROGRAMPROC glLinkProgram;
PFNGLGETPROGRAMIVPROC glGetProgramiv;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
PFNGLUSEPROGRAMPROC glUseProgram;
PFNGLDELETESHADERPROC glDeleteShader;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
PFNGLDEBUGMESSAGECALLBACKPROC glDebugMessageCallback;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
PFNGLUNIFORM1FPROC glUniform1f;
PFNGLUNIFORM4FPROC glUniform4f;

LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void ResizeGLViewport(HWND window)
{
    RECT clientRect;
    GetClientRect(window, &clientRect);
    glViewport(0, 0, clientRect.right, clientRect.bottom);
}

void Win32ProcessMessages(bool *running, bool *wireframeMode)
{
    MSG message;
    while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE))
    {
        switch (message.message)
        {
        case WM_QUIT:
            *running = false;
            return;
        case WM_KEYDOWN: {
            u32 vkCode = (u32)message.wParam;
            switch (vkCode)
            {
            case VK_ESCAPE:
                *running = false;
                return;
            case 'W':
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
    WCHAR windowClassName[] = L"DummyOGLExtensionsWindow";

    WNDCLASSW windowClass = {};
    windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    windowClass.lpfnWndProc = DefWindowProcW;
    windowClass.hInstance = hInstance;
    windowClass.lpszClassName = windowClassName;

    if (!RegisterClassW(&windowClass))
    {
        return -1;
    }

    HWND dummyWindow = CreateWindowW(windowClassName,                   // lpClassName,
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
    glGenBuffers = (PFNGLGENBUFFERSPROC)wglGetProcAddress("glGenBuffers");
    glBindBuffer = (PFNGLBINDBUFFERPROC)wglGetProcAddress("glBindBuffer");
    glBufferData = (PFNGLBUFFERDATAPROC)wglGetProcAddress("glBufferData");
    glCreateShader = (PFNGLCREATESHADERPROC)wglGetProcAddress("glCreateShader");
    glShaderSource = (PFNGLSHADERSOURCEPROC)wglGetProcAddress("glShaderSource");
    glCompileShader = (PFNGLCOMPILESHADERPROC)wglGetProcAddress("glCompileShader");
    glGetShaderiv = (PFNGLGETSHADERIVPROC)wglGetProcAddress("glGetShaderiv");
    glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)wglGetProcAddress("glGetShaderInfoLog");
    glCreateProgram = (PFNGLCREATEPROGRAMPROC)wglGetProcAddress("glCreateProgram");
    glAttachShader = (PFNGLATTACHSHADERPROC)wglGetProcAddress("glAttachShader");
    glLinkProgram = (PFNGLLINKPROGRAMPROC)wglGetProcAddress("glLinkProgram");
    glGetProgramiv = (PFNGLGETPROGRAMIVPROC)wglGetProcAddress("glGetProgramiv");
    glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)wglGetProcAddress("glGetProgramInfoLog");
    glUseProgram = (PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram");
    glDeleteShader = (PFNGLDELETESHADERPROC)wglGetProcAddress("glDeleteShader");
    glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)wglGetProcAddress("glVertexAttribPointer");
    glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)wglGetProcAddress("glEnableVertexAttribArray");
    glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)wglGetProcAddress("glGenVertexArrays");
    glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)wglGetProcAddress("glBindVertexArray");
    glDebugMessageCallback = (PFNGLDEBUGMESSAGECALLBACKPROC)wglGetProcAddress("glDebugMessageCallback");
    glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)wglGetProcAddress("glGetUniformLocation");
    glUniform1f = (PFNGLUNIFORM1FPROC)wglGetProcAddress("glUniform1f");
    glUniform4f = (PFNGLUNIFORM4FPROC)wglGetProcAddress("glUniform4f");

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

bool CompileShader(u32 *shaderID, GLenum shaderType, const char **shaderSource)
{
    *shaderID = glCreateShader(shaderType);
    glShaderSource(*shaderID, 1, shaderSource, NULL);
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

bool CreateShaderProgram(u32 *programID, u32 vertexShaderID, u32 fragmentShaderID)
{
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

    return true;
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
    windowClass.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1);
    windowClass.lpszClassName = windowClassName;

    RegisterClassW(&windowClass);

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
                                NULL                 // lpParam
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

#ifndef NDEBUG
        // glDebugMessageCallback(&DebugCallback, NULL);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif

        ResizeGLViewport(window);

        // MessageBoxA(NULL, (char *)glGetString(GL_VERSION), "OpenGL version", MB_OK);

        LARGE_INTEGER perfCounterFrequency;
        QueryPerformanceFrequency(&perfCounterFrequency);
        f32 perfCountDiffMultiplier = 1000.f / perfCounterFrequency.QuadPart;

        f32 targetFrameTime = 1000.f / 60;
        f32 deltaTime = targetFrameTime;
        f32 frameTimeAccumulator = 0.f;

        u64 lastFrameCount = Win32GetWallClock();

        // Shader initialization.
        u32 vertexShader;
        if (!CompileShader(&vertexShader, GL_VERTEX_SHADER, &vertexShaderSource))
        {
            return -1;
        }

        u32 fragmentShader;
        if (!CompileShader(&fragmentShader, GL_FRAGMENT_SHADER, &fragmentShaderSource))
        {
            return -1;
        }

        u32 shaderProgram;
        if (!CreateShaderProgram(&shaderProgram, vertexShader, fragmentShader))
        {
            return -1;
        }

        glDeleteShader(fragmentShader);

        u32 fragmentShader2;
        if (!CompileShader(&fragmentShader2, GL_FRAGMENT_SHADER, &fragmentShader2Source))
        {
            return -1;
        }

        u32 shaderProgram2;
        if (!CreateShaderProgram(&shaderProgram2, vertexShader, fragmentShader2))
        {
            return -1;
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader2);

        // Uncomment for triangle:
        float vertices[] = {
            -.5f, -.5f, 0.f, // Point 1.
            // 1.f,  0.f, 0.f, // Color 1.
            .5f, -.5f, 0.f, // Point 2.
            // 0.f,  1.f, 0.f, // Color 2.
            0.f, .5f, 0.f, // Point 3.
            // 0.f,  0.f, 1.f, // Color 3.
        };

        // Create and bind VAO 1.
        u32 vao1;
        glGenVertexArrays(1, &vao1);
        glBindVertexArray(vao1);

        // Create and bind VBO 1.
        u32 vbo1;
        glGenBuffers(1, &vbo1);
        glBindBuffer(GL_ARRAY_BUFFER, vbo1);

        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
        // glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(0);
        // glEnableVertexAttribArray(1);

        // Create and bind VAO 2.
        u32 vao2;
        glGenVertexArrays(1, &vao2);
        glBindVertexArray(vao2);

        // Create and bind VBO 2.
        u32 vbo2;
        glGenBuffers(1, &vbo2);
        glBindBuffer(GL_ARRAY_BUFFER, vbo2);

        float vertices2[] = {
            .5f, 0.f,  0.f, // Point 4.
            .5f, -.5f, 0.f, // Point 5.
            0.f, 0.f,  0.f  // Point 6.
        };

        // glBufferData(GL_ARRAY_BUFFER, sizeof(vertices) / 2, vertices + sizeof(vertices) / 2, GL_STATIC_DRAW);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices2), vertices2, GL_STATIC_DRAW);

        /*
        f32 rectVertices[] = {
            .5f,  .5f,  0.f, // 0: top right.
            .5f,  -.5f, 0.f, // 1: bottom right.
            -.5f, -.5f, 0.f, // 2: bottom left.
            -.5f, .5f,  0.f  // 3: top left.
        };

        u32 rectIndices[] = {
            0, 1, 3, // Triangle 1.
            1, 2, 3  // Triangle 2.
        };

        // Assume VAO and VBO already bound by this point.
        glBufferData(GL_ARRAY_BUFFER, sizeof(rectVertices), rectVertices, GL_STATIC_DRAW);

        u32 ebo;
        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(rectIndices), rectIndices, GL_STATIC_DRAW);
        */

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);

        int numAttribs;
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &numAttribs);
        WCHAR maxNumAttribsString[64];
        swprintf_s(maxNumAttribsString, L"Max number of vertex attributes supported: %i", numAttribs);
        MessageBoxW(NULL, maxNumAttribsString, L"OpenGL info", MB_OK);

        bool running = true;
        bool wireframeMode = false;
        while (running)
        {
            Win32ProcessMessages(&running, &wireframeMode);

            u64 currentFrameCount = Win32GetWallClock();
            u64 diff = currentFrameCount - lastFrameCount;
            lastFrameCount = currentFrameCount;

            deltaTime = (f32)diff * perfCountDiffMultiplier;

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

                glClearColor(.2f, .3f, .3f, 1.f);
                glClear(GL_COLOR_BUFFER_BIT);

                // glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

                glUseProgram(shaderProgram);
                glBindVertexArray(vao1);
                glDrawArrays(GL_TRIANGLES, 0, 3);

                glUseProgram(shaderProgram2);
                glBindVertexArray(vao2);
                glDrawArrays(GL_TRIANGLES, 0, 3);

                if (!SwapBuffers(hdc))
                {
                    if (MessageBoxW(window, L"Failed to swap buffers", L"OpenGL error", MB_OK) == S_OK)
                    {
                        running = false;
                    }
                }
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
    switch (uMsg)
    {
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
    case WM_SIZE:
        ResizeGLViewport(hWnd);
        return 0;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

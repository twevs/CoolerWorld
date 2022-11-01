#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <wingdi.h>
#include <winuser.h>
#include <stdint.h>
#include <stdio.h>

#include <GL/Gl.h>
#include "glcorearb.h"
#include "wglext.h"

#define global_variable static
#define internal static
#define local_persist static

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
                                 "void main()\n"
                                 "{\n"
                                 "    gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.f);\n"
                                 "}\0";
const char *fragmentShaderSource = "#version 330 core\n"
                                   "out vec4 fragColor;\n"
                                   "void main()\n"
                                   "{\n"
                                   "    fragColor = vec4(1.f, .5f, .2f, 1.f);\n"
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

LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void ResizeGLViewport(HWND window)
{
    RECT clientRect;
    GetClientRect(window, &clientRect);
    glViewport(0, 0, clientRect.right, clientRect.bottom);
}

void Win32ProcessMessages(bool *running)
{
    MSG message;
    while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE))
    {
        switch (message.message)
        {
        case WM_QUIT:
            *running = false;
            break;
        case WM_KEYDOWN: {
            u32 vkCode = (u32)message.wParam;
            switch (vkCode)
            {
            case VK_ESCAPE:
                *running = false;
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

    wglMakeCurrent(dummyDC, 0);
    wglDeleteContext(dummyRenderingContext);
    ReleaseDC(dummyWindow, dummyDC);
    DestroyWindow(dummyWindow);

    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    if (InitializeOpenGLExtensions(hInstance) == -1)
    {
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
            return -1;
        }

        PIXELFORMATDESCRIPTOR pixelFormatDescriptor = {};
        pixelFormatDescriptor.nSize = sizeof(PIXELFORMATDESCRIPTOR);
        if (!DescribePixelFormat(hdc, format, sizeof(PIXELFORMATDESCRIPTOR), &pixelFormatDescriptor))
        {
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
            return -1;
        }

        if (!wglMakeCurrent(hdc, renderingContext))
        {
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

        float vertices[] = {
            -.5f, -.5f, .0f, // Point 1.
            .5f,  -.5f, .0f, // Point 2.
            .0f,  .5f,  .0f  // Point 3.
        };

        u32 vao;
        glGenVertexArrays(1, &vao);

        glBindVertexArray(vao);

        u32 vbo;
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        u32 vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader);

        int success;
        char infoLog[512];
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
            printf("Shader compilation failed: %s\n", infoLog);
            return -1;
        }

        u32 fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);

        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
            printf("Shader compilation failed: %s\n", infoLog);
            return -1;
        }

        u32 shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);

        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
            return -1;
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);

        bool running = true;
        while (running)
        {
            Win32ProcessMessages(&running);

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
                glUseProgram(shaderProgram);
                glBindVertexArray(vao);
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

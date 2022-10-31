#include <Windows.h>
#include <wingdi.h>
#include <winuser.h>
#include <GL/Gl.h>
#include <stdint.h>
#include <stdio.h>

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

LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void ResizeGLViewport(HWND window)
{
    RECT clientRect;
    GetClientRect(window, &clientRect);
    glViewport(0, 0, clientRect.right, clientRect.bottom);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
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

        int pixelFormat = ChoosePixelFormat(hdc, &pfd);
        if (pixelFormat == 0)
        {
            return -1;
        }

        SetPixelFormat(hdc, pixelFormat, &pfd);

        HGLRC glRenderingContext = wglCreateContext(hdc);
        if (!glRenderingContext)
        {
            return -1;
        }

        if (!wglMakeCurrent(hdc, glRenderingContext))
        {
            return -1;
        }

        ResizeGLViewport(window);

        // MessageBoxA(NULL, (char *)glGetString(GL_VERSION), "OpenGL version", MB_OK);

        LARGE_INTEGER perfCounterFrequency;
        QueryPerformanceFrequency(&perfCounterFrequency);
        f32 perfCountDiffMultiplier = 1000.f / perfCounterFrequency.QuadPart;
        f32 targetFrameTime = 1000.f / 60;

        LARGE_INTEGER startCount;
        QueryPerformanceCounter(&startCount);

        bool running = true;
        while (running)
        {
            MSG message;
            while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE))
            {
                switch (message.message)
                {
                case WM_QUIT:
                    running = false;
                    break;
                case WM_KEYDOWN: {
                    u32 vkCode = (u32)message.wParam;
                    switch (vkCode)
                    {
                    case VK_ESCAPE:
                        running = false;
                    }
                    break;
                }
                default:
                    TranslateMessage(&message);
                    DispatchMessage(&message);
                }
            }

            glClearColor(.2f, .3f, .3f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT);

            if (!SwapBuffers(hdc))
            {
                if (MessageBoxW(window, L"Failed to swap buffers", L"OpenGL error", MB_OK) == S_OK)
                {
                    running = false;
                }
            }

            LARGE_INTEGER endCount;
            QueryPerformanceCounter(&endCount);
            u64 diff = endCount.QuadPart - startCount.QuadPart;
            f32 frameTime = (f32)diff * perfCountDiffMultiplier;
            if (frameTime < targetFrameTime)
            {
                // TODO: find a way to get a fixed 60 fps framerate as Sleep() has awful granularity.
                Sleep((u32)(targetFrameTime - frameTime) / 2);

                QueryPerformanceCounter(&endCount);
                diff = endCount.QuadPart - startCount.QuadPart;
                frameTime = (f32)diff * perfCountDiffMultiplier;
            }

            WCHAR frameTimeString[32];
            swprintf_s(frameTimeString, L"Frame time: %f ms\n", frameTime);
            OutputDebugStringW(frameTimeString);

            startCount = endCount;
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

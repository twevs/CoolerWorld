#include "arena.h"
#include "common.h"

typedef void (*InitializeTracyGPUContext_t)();
InitializeTracyGPUContext_t InitializeTracyGPUContext;

typedef bool (*InitializeImGuiInModule_t)(HWND window);
InitializeImGuiInModule_t InitializeImGuiInModule;

typedef ImGuiIO *(*GetImGuiIO_t)();
GetImGuiIO_t GetImGuiIO;

typedef LRESULT (*ImGui_WndProcHandler_t)(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
ImGui_WndProcHandler_t ImGui_WndProcHandler;

typedef bool (*InitializeDrawingInfo_t)(HWND window, TransientDrawingInfo *transientInfo,
                                        PersistentDrawingInfo *drawingInfo, CameraInfo *cameraInfo,
                                        Arena *texturesArena, Arena *meshDataArena);
InitializeDrawingInfo_t InitializeDrawingInfo;

typedef bool (*SaveDrawingInfo_t)(TransientDrawingInfo *transientInfo, PersistentDrawingInfo *drawingInfo,
                                  CameraInfo *cameraInfo);
SaveDrawingInfo_t SaveDrawingInfo;

typedef void (*ProvideCameraVectors_t)(CameraInfo *cameraInfo);
ProvideCameraVectors_t ProvideCameraVectors;

typedef void (*DrawWindow_t)(HWND window, HDC hdc, bool *running, TransientDrawingInfo *transientInfo,
                             PersistentDrawingInfo *drawingInfo, CameraInfo *cameraInfo, Arena *listArena,
                             Arena *tempArena);
DrawWindow_t DrawWindow;

typedef void (*PrintDepthTestFunc_t)(u32 val, char *outputBuffer, u32 bufSize);
PrintDepthTestFunc_t PrintDepthTestFunc;

PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;

PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;
PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;

LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

internal void ResizeGLViewport(HWND window, CameraInfo *cameraInfo, TransientDrawingInfo *transientInfo,
                               PersistentDrawingInfo *persistentInfo)
{
    RECT clientRect;
    GetClientRect(window, &clientRect);
    s32 width = clientRect.right;
    s32 height = clientRect.bottom;
    if (height == 0)
    {
        return;
    }
    glViewport(0, 0, width, height);
    cameraInfo->aspectRatio = (f32)width / (f32)height;

    if (persistentInfo->initialized)
    {
        // TODO: move this code into the DLL.
        for (u32 i = 0; i < 2; i++)
        {
            glBindTexture(GL_TEXTURE_2D, transientInfo->mainQuads[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        glBindRenderbuffer(GL_RENDERBUFFER, transientInfo->mainRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

        // See also: note about rear-view quad in DrawWindow().
        /*
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, transientInfo->rearViewQuad);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, transientInfo->numSamples, GL_RGB, width,
        height, GL_TRUE); glBindRenderbuffer(GL_RENDERBUFFER, transientInfo->rearViewRBO);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, transientInfo->numSamples,
        GL_DEPTH24_STENCIL8, width, height);
        */

        for (u32 i = 0; i < 2; i++)
        {
            glBindTexture(GL_TEXTURE_2D, transientInfo->mainQuads[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        glBindRenderbuffer(GL_RENDERBUFFER, transientInfo->mainRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

        glBindTexture(GL_TEXTURE_2D, transientInfo->postProcessingQuad);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
        glBindRenderbuffer(GL_RENDERBUFFER, transientInfo->postProcessingRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

        glBindTexture(GL_TEXTURE_2D, transientInfo->dirShadowMapQuad);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, DIR_SHADOW_MAP_SIZE, DIR_SHADOW_MAP_SIZE, 0,
                     GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

        for (u32 i = 0; i < 2; i++)
        {
            glBindTexture(GL_TEXTURE_2D, transientInfo->gaussianQuads[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
            glBindRenderbuffer(GL_RENDERBUFFER, transientInfo->gaussianRBOs[i]);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        }
    }
}

internal f32 clampf(f32 x, f32 min, f32 max, f32 safety = 0.f)
{
    return (x < min + safety) ? (min + safety) : (x > max - safety) ? (max - safety) : x;
}

internal void Win32ProcessMessages(HWND window, bool *running, PersistentDrawingInfo *drawingInfo, glm::vec3 *movement,
                                   CameraInfo *cameraInfo)
{
    MSG message;
    while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE))
    {
        if (ImGui_WndProcHandler(window, message.message, message.wParam, message.lParam))
        {
            break;
        }
        ImGuiIO *imGuiIO = GetImGuiIO();
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
                cameraInfo->yaw -= (xCoord - centreX) / 100.f;
                cameraInfo->pitch = clampf(cameraInfo->pitch - (yCoord - centreY) / 100.f, -PI / 2, PI / 2, .01f);

                SetCursorPos(centreX, centreY);
            }
            break;
        }
        case WM_MOUSEWHEEL: {
            s16 wheelRotation = HIWORD(message.wParam);
            bool shiftPressed = (GetKeyState(VK_SHIFT) < 0);
            if (shiftPressed)
            {
                cameraInfo->fov = clampf(cameraInfo->fov - (f32)wheelRotation * 3.f / WHEEL_DELTA, 0.f, 90.f);
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
            *movement += cameraInfo->forwardVector * deltaZ * speed;
            *movement += cameraInfo->rightVector * deltaX * speed;

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
                cameraInfo->aspectRatio = fmax(cameraInfo->aspectRatio - .1f, 0.f);
                break;
            case 'E':
                cameraInfo->aspectRatio = fmin(cameraInfo->aspectRatio + .1f, 10.f);
                break;
            case 'X':
                drawingInfo->wireframeMode = !drawingInfo->wireframeMode;
                glPolygonMode(GL_FRONT_AND_BACK, drawingInfo->wireframeMode ? GL_LINE : GL_FILL);
                break;
            case 'U': {
                s32 depthFunc;
                glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
                depthFunc = intMax(depthFunc - 1, 0x200);
                glDepthFunc(depthFunc);
                break;
            }
            case 'I': {
                s32 depthFunc;
                glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
                depthFunc = intMin(depthFunc + 1, 0x207);
                glDepthFunc(depthFunc);
                break;
            }
            case 'O':
                break;
            case 'J':
                break;
            case 'K':
                break;
            case 'L':
                break;
            case 'F': {
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

internal int InitializeOpenGLExtensions(HINSTANCE hInstance)
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

    HWND dummyWindow = CreateWindowW(dummyWindowClassName,              // lpClassName,
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

void LoadRenderingCode(HWND window)
{
    HMODULE loglLib = GetModuleHandleW(L"logl_runtime.dll");
    if (loglLib != NULL)
    {
        FreeLibrary(loglLib);
    }
    CopyFileW(L"logl.dll", L"logl_runtime.dll", FALSE);
    loglLib = LoadLibraryW(L"logl_runtime.dll");
    myAssert(loglLib != NULL);

    InitializeTracyGPUContext = (InitializeTracyGPUContext_t)GetProcAddress(loglLib, "InitializeTracyGPUContext");
    InitializeImGuiInModule = (InitializeImGuiInModule_t)GetProcAddress(loglLib, "InitializeImGuiInModule");
    InitializeImGuiInModule(window);
    GetImGuiIO = (GetImGuiIO_t)GetProcAddress(loglLib, "GetImGuiIO");
    ImGui_WndProcHandler = (ImGui_WndProcHandler_t)GetProcAddress(loglLib, "ImGui_WndProcHandler");

    DrawWindow = (DrawWindow_t)GetProcAddress(loglLib, "DrawWindow");
    InitializeDrawingInfo = (InitializeDrawingInfo_t)GetProcAddress(loglLib, "InitializeDrawingInfo");
    SaveDrawingInfo = (SaveDrawingInfo_t)GetProcAddress(loglLib, "SaveDrawingInfo");

    PrintDepthTestFunc = (PrintDepthTestFunc_t)GetProcAddress(loglLib, "PrintDepthTestFunc");

    ProvideCameraVectors = (ProvideCameraVectors_t)GetProcAddress(loglLib, "ProvideCameraVectors");
}

void CheckForNewDLL(HWND window, FILETIME *lastFileTime)
{
    HANDLE renderingDLL = CreateFileW(L"logl.dll", GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    FILETIME fileTime = {};
    GetFileTime(renderingDLL, 0, 0, &fileTime);
    CloseHandle(renderingDLL);
    if (CompareFileTime(lastFileTime, &fileTime) != 0)
    {
        LoadRenderingCode(window);
    }
    *lastFileTime = fileTime;
}

void GLAPIENTRY DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                              const GLchar *message, const void *userParam)
{
    if (source == GL_DEBUG_SOURCE_APPLICATION)
    {
        return;
    }
    DebugPrintA(message);
    myAssert(false);
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
                                4,
                                WGL_CONTEXT_MINOR_VERSION_ARB,
                                6,
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
            OutputDebugStringW(L"Failed to make the created OpenGL rendering context the hardware "
                               L"device context's "
                               L"current rendering context.");
            return -1;
        }

        wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
        if (!wglSwapIntervalEXT(1))
        {
            OutputDebugStringW(L"Failed to make the created OpenGL rendering context the hardware "
                               L"device context's "
                               L"current rendering context.");
            return -1;
        }

        if (glewInit() != GLEW_OK)
        {
            __debugbreak();
        }

#ifndef NDEBUG
        glDebugMessageCallback(&DebugCallback, NULL);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif

        ResizeGLViewport(window, &appState.cameraInfo, &appState.transientInfo, &appState.persistentInfo);

        // MessageBoxA(NULL, (char *)glGetString(GL_VERSION), "OpenGL version", MB_OK);

        // Shader initialization.

        LoadRenderingCode(window);
        InitializeTracyGPUContext();

        TransientDrawingInfo *transientInfo = &appState.transientInfo;
        PersistentDrawingInfo *drawingInfo = &appState.persistentInfo;
        CameraInfo *cameraInfo = &appState.cameraInfo;
        Arena *texturesArena = AllocArena(1024);
        Arena *meshesArena = AllocArena(100 * sizeof(Mesh));
        Arena *meshDataArena = AllocArena(100 * 1024 * 1024);
        Arena *listArena = AllocArena(2048);
        Arena *tempArena = AllocArena(1920 * 1080 * 32);
        if (!InitializeDrawingInfo(window, transientInfo, drawingInfo, cameraInfo, texturesArena, meshDataArena))
        {
            return -1;
        }

        glm::vec3 movementPerFrame = {};

        f32 targetFrameTime = 1000.f / 60;
        f32 deltaTime = targetFrameTime;

        u64 lastFrameCount = Win32GetWallClock();

        HANDLE renderingDLL = CreateFileW(L"logl.dll", GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
        FILETIME lastFileTime = {};
        GetFileTime(renderingDLL, 0, 0, &lastFileTime);
        CloseHandle(renderingDLL);

        f32 dllAccumulator = 0.f;

        appState.running = true;
        while (appState.running)
        {
            dllAccumulator += deltaTime;
            if (dllAccumulator >= 50.f)
            {
                CheckForNewDLL(window, &lastFileTime);
                dllAccumulator = 0.f;
            }

            ProvideCameraVectors(cameraInfo);

            Win32ProcessMessages(window, &appState.running, drawingInfo, &movementPerFrame, cameraInfo);

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

            // TODO: investigate whether it's worth keeping this around as a framerate cap
            // mechanism. Won't it produce screen-tearing since it won't necessarily be synchronized
            // with the v-blank? How many users actually disable v-sync anyway?

            // WCHAR frameTimeString[32];
            // swprintf_s(frameTimeString, L"Frame time: %f ms\n", frameTimeAccumulator);
            // OutputDebugStringW(frameTimeString);

            cameraInfo->pos += movementPerFrame * deltaTime;
            DrawWindow(window, hdc, &appState.running, transientInfo, drawingInfo, cameraInfo, listArena, tempArena);
            movementPerFrame = glm::vec3(0.f);
            // DebugPrintA("Camera pitch: %f\n", cameraInfo->pitch);
            // DebugPrintA("Camera yaw: %f\n", cameraInfo->yaw);

            struct RingBuffer
            {
                f32 deltaTimes[60];
                u32 index = 0;
            };
            local_persist RingBuffer deltaTimeBuffer;
            deltaTimeBuffer.deltaTimes[deltaTimeBuffer.index] = deltaTime;
            deltaTimeBuffer.index = (deltaTimeBuffer.index + 1) % 60;
            f32 totalDeltaTime = 0.f;
            for (u32 i = 0; i < 60; i++)
            {
                totalDeltaTime += deltaTimeBuffer.deltaTimes[i];
            }
            f32 averageDeltaTime = totalDeltaTime / 60;
            DebugPrintA("averageDeltaTime: %f\n", averageDeltaTime);
        }
    }

    SaveDrawingInfo(&appState.transientInfo, &appState.persistentInfo, &appState.cameraInfo);

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
        ResizeGLViewport(hWnd, &appState->cameraInfo, &appState->transientInfo, &appState->persistentInfo);
        return 0;
    }
    case WM_ERASEBKGND:
        // See:
        // https://stackoverflow.com/questions/43670470/drawn-opengl-disappearing-on-window-resize
        return 1;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

#include "arena.h"
#include "common.h"

/***********************************************************************************************************************
 *
 * Function pointers assigned from game DLL at load/reload time.
 *
 **********************************************************************************************************************/

//
// Tracy.
//

typedef void (*InitializeTracyGPUContext_t)();
InitializeTracyGPUContext_t InitializeTracyGPUContext;

//
// Dear ImGui.
//

typedef bool (*InitializeImGuiInModule_t)(HWND window);
InitializeImGuiInModule_t InitializeImGuiInModule;

typedef ImGuiIO *(*GetImGuiIO_t)();
GetImGuiIO_t GetImGuiIO;

typedef LRESULT (*ImGui_WndProcHandler_t)(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
ImGui_WndProcHandler_t ImGui_WndProcHandler;

//
// Rendering.
//

typedef bool (*InitializeDrawingInfo_t)(HWND window, TransientDrawingInfo *transientInfo,
                                        PersistentDrawingInfo *drawingInfo, CameraInfo *cameraInfo);
InitializeDrawingInfo_t InitializeDrawingInfo;

typedef bool (*SaveDrawingInfo_t)(TransientDrawingInfo *transientInfo, PersistentDrawingInfo *drawingInfo,
                                  CameraInfo *cameraInfo);
SaveDrawingInfo_t SaveDrawingInfo;

// Main rendering function.
typedef void (*DrawWindow_t)(HWND window, HDC hdc, ApplicationState *appState, Arena *listArena, Arena *tempArena);
DrawWindow_t DrawWindow;

//
// Functions for viewport navigation and interaction.
//

// ProvideCameraVectors() fills cameraInfo with the camera's forward and right unit vectors.
typedef void (*ProvideCameraVectors_t)(CameraInfo *cameraInfo);
ProvideCameraVectors_t ProvideCameraVectors;

// GameHandleClick() handles screen picking, to add a cube to the clicked-on face of a cube already in the scene.
typedef void (*GameHandleClick_t)(TransientDrawingInfo *transientInfo, CWInput button, CWPoint coordinates,
                                  CWPoint screenSize);
GameHandleClick_t GameHandleClick;

//
// OpenGL.
//

PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;

PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;
PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;

/***********************************************************************************************************************
 *
 * Handle OpenGL viewport resizing, called upon initialization as well as from WndProc's WM_SIZE case.
 *
 **********************************************************************************************************************/

internal void ResizeGLViewport(HWND window, CameraInfo *cameraInfo, TransientDrawingInfo *transientInfo,
                               PersistentDrawingInfo *persistentInfo)
{
    // Get new client area size.
    RECT clientRect;
    GetClientRect(window, &clientRect);
    s32 width = clientRect.right;
    s32 height = clientRect.bottom;
    if (height == 0)
    {
        return;
    }


    // Resize GL viewport to match client area and update aspect ratio.
    glViewport(0, 0, width, height);
    cameraInfo->aspectRatio = (f32)width / (f32)height;

    // Resize our framebuffers.
    if (persistentInfo->initialized)
    {
        // TODO: fix this code and move it into the DLL.
        for (u32 i = 0; i < 2; i++)
        {
            glBindTexture(GL_TEXTURE_2D, transientInfo->mainFramebuffer.attachments[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        glBindRenderbuffer(GL_RENDERBUFFER, transientInfo->mainFramebuffer.rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

        // See also: note about rear-view quad in DrawWindow().
        /*
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, transientInfo->rearViewQuad);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, transientInfo->numSamples, GL_RGB, width,
        height, GL_TRUE); glBindRenderbuffer(GL_RENDERBUFFER, transientInfo->rearViewrbo);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, transientInfo->numSamples,
        GL_DEPTH24_STENCIL8, width, height);
        */

        glBindTexture(GL_TEXTURE_2D, transientInfo->postProcessingFramebuffer.attachments[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
        glBindRenderbuffer(GL_RENDERBUFFER, transientInfo->postProcessingFramebuffer.rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

        glBindTexture(GL_TEXTURE_2D, transientInfo->dirShadowMapFramebuffer.attachments[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, DIR_SHADOW_MAP_SIZE, DIR_SHADOW_MAP_SIZE, 0,
                     GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

        for (u32 i = 0; i < 2; i++)
        {
            glBindTexture(GL_TEXTURE_2D, transientInfo->gaussianFramebuffers[i].attachments[0]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
            glBindRenderbuffer(GL_RENDERBUFFER, transientInfo->gaussianFramebuffers[i].rbo);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        }
    }
}

/***********************************************************************************************************************
 *
 * Windows message loop processing, done from within the game loop.
 *
 **********************************************************************************************************************/

internal f32 clampf(f32 x, f32 min, f32 max, f32 safety = 0.f)
{
    return (x < min + safety) ? (min + safety) : (x > max - safety) ? (max - safety) : x;
}

internal void Win32ProcessMessages(HWND window, bool *running, PersistentDrawingInfo *persistentInfo,
                                   TransientDrawingInfo *transientInfo, glm::vec3 *movement, CameraInfo *cameraInfo)
{
    MSG message;
    while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE))
    {
        // Give Dear ImGui first dibs to ensure input processing is correctly handled.
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
        
        // Mouse parameters.
        local_persist bool capturing = false;
        local_persist f32 speed = .1f;

        // Retrieve window information.
        POINT windowOrigin = {};
        ClientToScreen(window, &windowOrigin);
        s16 originX = (s16)windowOrigin.x;
        s16 originY = (s16)windowOrigin.y;

        RECT clientRect;
        GetClientRect(window, &clientRect);
        s32 width = clientRect.right;
        s32 height = clientRect.bottom;
        s16 centreX = originX + (s16)width / 2;
        s16 centreY = originY + (s16)height / 2;

        switch (message.message)
        {
        case WM_QUIT:
            *running = false;
            break;
        case WM_LBUTTONDOWN: {
            // NOTE: received coordinates will be relative to the top left of the client area,
            // so we convert them into OpenGL's lower-left-origin coordinate system.
            CWPoint coordinates = {GET_X_LPARAM(message.lParam), height - GET_Y_LPARAM(message.lParam)};
            CWPoint screenSize = {width, height};
            GameHandleClick(transientInfo, CWInput::LeftButton, coordinates, screenSize);
        }
        break;
        case WM_RBUTTONDOWN:
            // Implement Unreal-style navigation, ie holding down the right mouse button causes the cursor to disappear
            // and enables rotation via mouse movement and viewport movement via WASD.
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
            // Implement Unreal-style modification of viewport movement speed via mouse wheel (scroll up to increase
            // speed, scroll down to decrease it).
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
            // Treat WASD specially, for the purposes of viewport movement when the right mouse button is held down (see
            // WM_RBUTTONDOWN case above).
            float deltaZ = (GetKeyState('W') < 0) ? .1f : (GetKeyState('S') < 0) ? -.1f : 0.f;
            float deltaX = (GetKeyState('D') < 0) ? .1f : (GetKeyState('A') < 0) ? -.1f : 0.f;
            *movement += cameraInfo->forwardVector * deltaZ * speed;
            *movement += cameraInfo->rightVector * deltaX * speed;

            // Handle other keys.
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
                    persistentInfo->spotLight.innerCutoff += .05f;
                    DebugPrintA("innerCutoff: %f\n", persistentInfo->spotLight.innerCutoff);
                }
                break;
            case VK_DOWN:
                if (altPressed)
                {
                }
                else
                {
                    persistentInfo->spotLight.innerCutoff -= .05f;
                    DebugPrintA("innerCutoff: %f\n", persistentInfo->spotLight.innerCutoff);
                }
                break;
            case VK_LEFT:
                persistentInfo->spotLight.outerCutoff -= .05f;
                DebugPrintA("outerCutoff: %f\n", persistentInfo->spotLight.outerCutoff);
                break;
            case VK_RIGHT:
                persistentInfo->spotLight.outerCutoff += .05f;
                DebugPrintA("outerCutoff: %f\n", persistentInfo->spotLight.outerCutoff);
                break;
            case 'Q':
                cameraInfo->aspectRatio = fmax(cameraInfo->aspectRatio - .1f, 0.f);
                break;
            case 'E':
                cameraInfo->aspectRatio = fmin(cameraInfo->aspectRatio + .1f, 10.f);
                break;
            case 'X':
                persistentInfo->wireframeMode = !persistentInfo->wireframeMode;
                glPolygonMode(GL_FRONT_AND_BACK, persistentInfo->wireframeMode ? GL_LINE : GL_FILL);
                break;
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
                persistentInfo->spotLight.ambient = flashLightOn ? glm::vec3(.1f) : glm::vec3(0.f);
                persistentInfo->spotLight.diffuse = flashLightOn ? glm::vec3(.5f) : glm::vec3(0.f);
                persistentInfo->spotLight.specular = flashLightOn ? glm::vec3(1.f) : glm::vec3(0.f);
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

/***********************************************************************************************************************
 *
 * Initialize modern OpenGL context.
 *
 **********************************************************************************************************************/

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

/***********************************************************************************************************************
 *
 * Hot reloading.
 *
 **********************************************************************************************************************/

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

    // Assign function pointers from game DLL.
    InitializeTracyGPUContext = (InitializeTracyGPUContext_t)GetProcAddress(loglLib, "InitializeTracyGPUContext");
    
    InitializeImGuiInModule = (InitializeImGuiInModule_t)GetProcAddress(loglLib, "InitializeImGuiInModule");
    InitializeImGuiInModule(window);
    GetImGuiIO = (GetImGuiIO_t)GetProcAddress(loglLib, "GetImGuiIO");
    ImGui_WndProcHandler = (ImGui_WndProcHandler_t)GetProcAddress(loglLib, "ImGui_WndProcHandler");
    
    InitializeDrawingInfo = (InitializeDrawingInfo_t)GetProcAddress(loglLib, "InitializeDrawingInfo");
    SaveDrawingInfo = (SaveDrawingInfo_t)GetProcAddress(loglLib, "SaveDrawingInfo");
    DrawWindow = (DrawWindow_t)GetProcAddress(loglLib, "DrawWindow");
    
    ProvideCameraVectors = (ProvideCameraVectors_t)GetProcAddress(loglLib, "ProvideCameraVectors");
    GameHandleClick = (GameHandleClick_t)GetProcAddress(loglLib, "GameHandleClick");
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

/***********************************************************************************************************************
 *
 * OpenGL debug callback.
 *
 **********************************************************************************************************************/

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

/***********************************************************************************************************************
 *
 * Entry point.
 *
 **********************************************************************************************************************/

LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    if (InitializeOpenGLExtensions(hInstance) == -1)
    {
        OutputDebugStringW(L"Failed to initialize OpenGL extensions.");
        return -1;
    }

    WCHAR windowClassName[] = L"CoolerWorldWindowClass";

    WNDCLASSW windowClass = {};
    windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    windowClass.lpfnWndProc = WndProc;
    windowClass.hInstance = hInstance;
    windowClass.hbrBackground = (HBRUSH)(NULL + 1);
    windowClass.lpszClassName = windowClassName;

    RegisterClassW(&windowClass);

    ApplicationState appState = {};

    HWND window = CreateWindowW(windowClassName,     // lpClassName,
                                L"Cooler World",     // lpWindowName,
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

        // Load rendering code and initialize Tracy context.
        LoadRenderingCode(window);
        InitializeTracyGPUContext();

        // Initialize drawing info.
        TransientDrawingInfo *transientInfo = &appState.transientInfo;
        PersistentDrawingInfo *persistentInfo = &appState.persistentInfo;
        CameraInfo *cameraInfo = &appState.cameraInfo;
        if (!InitializeDrawingInfo(window, transientInfo, persistentInfo, cameraInfo))
        {
            return -1;
        }

        // Initialize our game DLL timestamp for hot reloading purposes.
        HANDLE renderingDLL = CreateFileW(L"logl.dll", GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
        FILETIME lastFileTime = {};
        GetFileTime(renderingDLL, 0, 0, &lastFileTime);
        CloseHandle(renderingDLL);

        // Accumulates delta time every frame to allow setting a minimum interval on DLL timestamp checking.
        f32 dllAccumulator = 0.f;

        // Allocate memory arenas used on a per-frame basis.
        Arena *listArena = AllocArena(2048);
        Arena *tempArena = AllocArena(1920 * 1080 * 32);

        // Set up variables needed for tracking changes frame by frame.
        glm::vec3 movementPerFrame = {};
        f32 targetFrameTime = 1000.f / 60;
        f32 deltaTime = targetFrameTime;
        u64 lastFrameCount = Win32GetWallClock();

        // Main game loop.
        appState.running = true;
        while (appState.running)
        {
            // Periodically check for a new game DLL; if it has been rebuilt, CheckForNewDLL() will call
            // LoadRenderingCode() to perform hot reload.
            // TODO: not a huge fan of this side effect, a function called CheckForNewDLL() shouldn't also perform a
            // *load*. Make it return a bool that you act upon instead.
            dllAccumulator += deltaTime;
            if (dllAccumulator >= 50.f)
            {
                CheckForNewDLL(window, &lastFileTime);
                dllAccumulator = 0.f;
            }

            // Retrieve the camera's front and right unit vectors so we can pass them to our Windows message processor
            // for the purposes of viewport movement.
            ProvideCameraVectors(cameraInfo);

            // Process Windows messages.
            // movementPerFrame gets written to based on input received from Windows.
            Win32ProcessMessages(window, &appState.running, persistentInfo, transientInfo, &movementPerFrame,
                                 cameraInfo);

            // Calculate delta time.
            u64 currentFrameCount = Win32GetWallClock();
            u64 diff = currentFrameCount - lastFrameCount;
            lastFrameCount = currentFrameCount;
            deltaTime = (f32)diff * Win32GetWallClockPeriod();

            // Handle resuming from a breakpoint.
            if (deltaTime >= 1000.f)
            {
                deltaTime = targetFrameTime;
            }

            // Handle viewport movement and actually draw the scene at the new viewport position.
            cameraInfo->pos += movementPerFrame * deltaTime;
            DrawWindow(window, hdc, &appState, listArena, tempArena);
            movementPerFrame = glm::vec3(0.f);

            // Code to allow viewing the average framerate over the last 60 frames.
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
        }
    }

    // Save the programme's info when exiting.
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

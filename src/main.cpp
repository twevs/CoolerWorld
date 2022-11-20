#include "common.h"

typedef bool (*InitializeImGuiInModule_t)(HWND window);
InitializeImGuiInModule_t InitializeImGuiInModule;

typedef ImGuiIO * (*GetImGuiIO_t)();
GetImGuiIO_t GetImGuiIO;

typedef LRESULT (*ImGui_WndProcHandler_t)(
  HWND hWnd,
  UINT msg,
  WPARAM wParam,
  LPARAM lParam);
ImGui_WndProcHandler_t ImGui_WndProcHandler;

typedef bool (*SetShaderUniformSampler_t)(
            u32 shaderProgram,
            const char *uniformName,
            u32 slot);
SetShaderUniformSampler_t SetShaderUniformSampler;

typedef bool (*LoadDrawingInfo_t)(
            PersistentDrawingInfo *drawingInfo,
            CameraInfo *cameraInfo);
LoadDrawingInfo_t LoadDrawingInfo;

typedef bool (*SaveDrawingInfo_t)(
            PersistentDrawingInfo *drawingInfo,
            CameraInfo *cameraInfo);
SaveDrawingInfo_t SaveDrawingInfo;

typedef void (*ProvideCameraVectors_t)(
        CameraInfo *cameraInfo);
ProvideCameraVectors_t ProvideCameraVectors;

typedef void (*DrawWindow_t)(
            HWND window,
            HDC hdc,
            bool *running,
            TransientDrawingInfo *transientInfo,
            PersistentDrawingInfo *drawingInfo,
            CameraInfo *cameraInfo);
DrawWindow_t DrawWindow;

typedef void (*PrintDepthTestFunc_t)(
    u32 val,
    char *outputBuffer,
    u32 bufSize);
PrintDepthTestFunc_t PrintDepthTestFunc;

PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;

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

internal Arena *AllocArena(u64 size)
{
    void *mem = VirtualAlloc(NULL, sizeof(Arena) + size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Arena *newArena = (Arena *)mem;
    newArena->memory = (u8 *)mem + sizeof(Arena);
    newArena->size = size;
    return newArena;
}

internal void FreeArena(Arena *arena)
{
    VirtualFree(arena, arena->size, MEM_RELEASE);
}

internal void *ArenaPush(Arena *arena, u64 size)
{
    void *mem = (void *)((u8 *)arena->memory + arena->stackPointer);
    arena->stackPointer += size;
    myAssert(arena->stackPointer <= arena->size);
    return mem;
}

internal void ArenaPop(Arena *arena, u64 size)
{
    arena->stackPointer -= size;
    myAssert(arena->stackPointer >= 0);
}

internal void ArenaClear(Arena *arena)
{
    arena->stackPointer = 0;
}

PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;
PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;

LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

internal void DebugPrintA(const char *formatString, ...)
{
    CHAR debugString[1024];
    va_list args;
    va_start(args, formatString);
    vsprintf_s(debugString, formatString, args);
    va_end(args);
    OutputDebugStringA(debugString);
}

internal void ResizeGLViewport(HWND window)
{
    RECT clientRect;
    GetClientRect(window, &clientRect);
    glViewport(0, 0, clientRect.right, clientRect.bottom);
}

internal f32 clampf(f32 x, f32 min, f32 max, f32 safety = 0.f)
{
    return (x < min + safety) ? (min + safety) : (x > max - safety) ? (max - safety) : x;
}

internal void Win32ProcessMessages(
    HWND window,
    bool *running,
    PersistentDrawingInfo *drawingInfo,
    glm::vec3 *movement,
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
                cameraInfo->pitch = clampf(cameraInfo->pitch - (yCoord - centreY) / 100.f,
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
            case 'U':
                {
                    s32 depthFunc;
                    glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
                    depthFunc = max(depthFunc - 1, 0x200);
                    glDepthFunc(depthFunc);
                    break;
                }
            case 'I':
                {
                    s32 depthFunc;
                    glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
                    depthFunc = min(depthFunc + 1, 0x207);
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

internal bool CompileShader(u32 *shaderID, GLenum shaderType, const char *shaderFilename)
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

internal bool CreateShaderProgram(u32 *programID, const char *vertexShaderFilename, const char *fragmentShaderFilename)
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

internal u32 CreateTextureFromImage(const char *filename)
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

internal void LoadTextures(Mesh *mesh, u64 num, aiMaterial *material, aiTextureType type, Arena *texturesArena, LoadedTextures *loadedTextures)
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

internal Mesh ProcessMesh(aiMesh *mesh, const aiScene *scene, Arena *texturesArena, LoadedTextures *loadedTextures)
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

internal void ProcessNode(aiNode *node, const aiScene *scene, Mesh *meshes, u32 *meshCount, Arena *texturesArena, LoadedTextures *loadedTextures)
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

internal u32 CreateVAO(VaoInformation *vaoInfo)
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

internal Model LoadModel(const char *filename, s32 *elemCounts, u32 elemCountsSize)
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
    
    InitializeImGuiInModule = (InitializeImGuiInModule_t)GetProcAddress(loglLib, "InitializeImGuiInModule");
    InitializeImGuiInModule(window);
    GetImGuiIO = (GetImGuiIO_t)GetProcAddress(loglLib, "GetImGuiIO");
    ImGui_WndProcHandler = (ImGui_WndProcHandler_t)GetProcAddress(loglLib, "ImGui_WndProcHandler");
        
    DrawWindow = (DrawWindow_t)GetProcAddress(loglLib, "DrawWindow");
    
    LoadDrawingInfo = (LoadDrawingInfo_t)GetProcAddress(loglLib, "LoadDrawingInfo");
    SaveDrawingInfo = (SaveDrawingInfo_t)GetProcAddress(loglLib, "SaveDrawingInfo");
    
    SetShaderUniformSampler = (SetShaderUniformSampler_t)GetProcAddress(loglLib, "SetShaderUniformSampler");
    
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

#ifndef NDEBUG
        // glDebugMessageCallback(&DebugCallback, NULL);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif

        ResizeGLViewport(window);

        // MessageBoxA(NULL, (char *)glGetString(GL_VERSION), "OpenGL version", MB_OK);

        // Shader initialization.
        
        u64 arenaSize = 100 * 1024 * 1024;
        globalArena = *AllocArena(arenaSize);
        
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
        
        u32 outlineShaderProgram;
        if (!CreateShaderProgram(&outlineShaderProgram, "vertex_shader.vs", "outline.fs"))
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
        
        u32 cubeVao = CreateVAO(&vaoInfo);
        
        TransientDrawingInfo* transientInfo = &appState.transientInfo;
        transientInfo->objectShaderProgram = objectShaderProgram;
        transientInfo->lightShaderProgram = lightShaderProgram;
        transientInfo->outlineShaderProgram = outlineShaderProgram;
        transientInfo->cubeVao = cubeVao;
        transientInfo->backpack = LoadModel("backpack.obj", elemCounts, myArraySize(elemCounts));
        transientInfo->grassTexture = CreateTextureFromImage("grass.png");
        
        LoadRenderingCode(window);
        
        PersistentDrawingInfo *drawingInfo = &appState.persistentInfo;
        CameraInfo *cameraInfo = &appState.cameraInfo;
        if (!LoadDrawingInfo(drawingInfo, cameraInfo))
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
            
            for (u32 grassIndex = 0; grassIndex < NUM_GRASS; grassIndex++)
            {
                f32 x = (f32)(rand() % 10);
                x = (rand() > RAND_MAX / 2) ? x : -x;
                f32 y = (f32)(rand() % 10);
                y = (rand() > RAND_MAX / 2) ? y : -y;
                f32 z = (f32)(rand() % 10);
                z = (rand() > RAND_MAX / 2) ? z : -z;
                drawingInfo->grassPos[grassIndex] = { x, y, z };
            }
            
            drawingInfo->dirLight.direction =
                glm::normalize(pointLights[NUM_POINTLIGHTS - 1].position - pointLights[0].position);
        }
        
        drawingInfo->initialized = true;
        
        RECT clientRect;
        GetClientRect(window, &clientRect);
        f32 width = (f32)clientRect.right;
        f32 height = (f32)clientRect.bottom;
        cameraInfo->aspectRatio = width / height;
        
        glUseProgram(objectShaderProgram);
        SetShaderUniformSampler(objectShaderProgram, "material.diffuse", 0);
        SetShaderUniformSampler(objectShaderProgram, "material.specular", 1);
        
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
            
            Win32ProcessMessages(
                window,
                &appState.running,
                drawingInfo,
                &movementPerFrame,
                cameraInfo);
            
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
        
            cameraInfo->pos += movementPerFrame * deltaTime;
            DrawWindow(window, hdc, &appState.running, transientInfo, drawingInfo, cameraInfo);
            movementPerFrame = glm::vec3(0.f);
            // DebugPrintA("Camera pitch: %f\n", cameraInfo->pitch);
            // DebugPrintA("Camera yaw: %f\n", cameraInfo->yaw);
        }
    }
    
    SaveDrawingInfo(&appState.persistentInfo, &appState.cameraInfo);

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

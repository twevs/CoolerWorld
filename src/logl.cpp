#include "common.h"
#include "skiplist.h"

global_variable ImGuiContext *imGuiContext;
global_variable ImGuiIO *imGuiIO;

extern "C" __declspec(dllexport) void InitializeImGuiInModule(HWND window)
{
    IMGUI_CHECKVERSION();
    imGuiContext = ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    imGuiIO = &io;
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(window);
    ImGui_ImplOpenGL3_Init("#version 450");
}

extern "C" __declspec(dllexport) ImGuiIO *GetImGuiIO()
{
    return imGuiIO;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

extern "C" __declspec(dllexport) LRESULT ImGui_WndProcHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    return ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
}

extern "C" __declspec(dllexport) void PrintDepthTestFunc(u32 val, char *outputBuffer, u32 bufSize)
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
        DebugPrintA("Shader compilation failed for %s: %s\n", shaderFilename, infoLog);
        return false;
    }

    CloseHandle(file);
    return true;
}

internal bool CreateShaderProgram(ShaderProgram *program)
{
    u32 vertexShaderID = 0;
    if (!CompileShader(&vertexShaderID, GL_VERTEX_SHADER, program->vertexShaderFilename))
    {
        return false;
    }

    u32 fragmentShaderID = 0;
    if (!CompileShader(&fragmentShaderID, GL_FRAGMENT_SHADER, program->fragmentShaderFilename))
    {
        return false;
    }

    u32 *programID = &program->id;
    if (*programID == 0)
    {
        *programID = glCreateProgram();
    }
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

internal FILETIME GetFileTime(const char *filename)
{
    HANDLE file = CreateFileA(filename, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    FILETIME fileTime = {};
    GetFileTime(file, 0, 0, &fileTime);
    CloseHandle(file);

    return fileTime;
}

internal void SetShaderUniformSampler(u32 shaderProgram, const char *uniformName, u32 slot)
{
    glUniform1i(glGetUniformLocation(shaderProgram, uniformName), slot);
}

internal void SetShaderUniformFloat(u32 shaderProgram, const char *uniformName, float value)
{
    glUniform1f(glGetUniformLocation(shaderProgram, uniformName), value);
}

internal void SetShaderUniformVec3(u32 shaderProgram, const char *uniformName, glm::vec3 vector)
{
    glUniform3fv(glGetUniformLocation(shaderProgram, uniformName), 1, glm::value_ptr(vector));
}

internal void SetShaderUniformVec4(u32 shaderProgram, const char *uniformName, glm::vec4 vector)
{
    glUniform4fv(glGetUniformLocation(shaderProgram, uniformName), 1, glm::value_ptr(vector));
}

internal void SetShaderUniformMat3(u32 shaderProgram, const char *uniformName, glm::mat3 *matrix)
{
    glUniformMatrix3fv(glGetUniformLocation(shaderProgram, uniformName), 1, GL_FALSE, glm::value_ptr(*matrix));
}

internal void SetShaderUniformMat4(u32 shaderProgram, const char *uniformName, glm::mat4 *matrix)
{
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, uniformName), 1, GL_FALSE, glm::value_ptr(*matrix));
}

internal bool CreateShaderProgram(ShaderProgram *program, u32 id, const char *vertexShaderFilename,
                                  const char *fragmentShaderFilename)
{
    ShaderProgram newShaderProgram = {};
    strcpy(newShaderProgram.vertexShaderFilename, vertexShaderFilename);
    strcpy(newShaderProgram.fragmentShaderFilename, fragmentShaderFilename);
    newShaderProgram.vertexShaderTime = GetFileTime(vertexShaderFilename);
    newShaderProgram.fragmentShaderTime = GetFileTime(fragmentShaderFilename);
    if (!CreateShaderProgram(&newShaderProgram))
    {
        return false;
    }
    if (glIsProgram(id))
    {
        glDeleteProgram(id);
    }
    
    *program = newShaderProgram;
    return true;
}

internal bool CreateShaderPrograms(TransientDrawingInfo *info)
{
    if (!CreateShaderProgram(&info->objectShader, info->objectShader.id, "vertex_shader.vs", "fragment_shader.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->lightShader, info->lightShader.id, "vertex_shader.vs", "light_fragment_shader.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->outlineShader, info->outlineShader.id, "vertex_shader.vs", "outline.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->textureShader, info->textureShader.id, "vertex_shader.vs", "texture.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->glassShader, info->glassShader.id, "vertex_shader.vs", "glass.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->postProcessShader, info->postProcessShader.id, "vertex_shader.vs",
                             "postprocess.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->skyboxShader, info->skyboxShader.id, "cubemap.vs", "cubemap.fs"))
    {
        return false;
    }

    glUseProgram(info->objectShader.id);
    SetShaderUniformSampler(info->objectShader.id, "material.diffuse", 0);
    SetShaderUniformSampler(info->objectShader.id, "material.specular", 1);
    SetShaderUniformSampler(info->objectShader.id, "skybox", 2);

    glUseProgram(info->textureShader.id);
    SetShaderUniformSampler(info->textureShader.id, "tex", 0);

    glUseProgram(info->glassShader.id);
    SetShaderUniformSampler(info->glassShader.id, "tex", 0);
    SetShaderUniformSampler(info->glassShader.id, "skybox", 1);

    glUseProgram(info->skyboxShader.id);
    SetShaderUniformSampler(info->skyboxShader.id, "skybox", 0);

    return true;
}

internal bool ReloaderShaderPrograms(TransientDrawingInfo *info)
{
    myAssert(glIsProgram(info->objectShader.id));
    myAssert(glIsProgram(info->lightShader.id));
    myAssert(glIsProgram(info->outlineShader.id));
    myAssert(glIsProgram(info->glassShader.id));
    myAssert(glIsProgram(info->textureShader.id));
    myAssert(glIsProgram(info->postProcessShader.id));
    myAssert(glIsProgram(info->skyboxShader.id));

    u32 shaders[2];
    s32 count;

    glGetAttachedShaders(info->objectShader.id, 2, &count, shaders);
    myAssert(count == 2);
    // glShaderSource(shaders[0], 1, objectVertexShader, NULL);
    // glShaderSource(shaders[0], 1, objectFragmentShader, NULL);

    glGetAttachedShaders(info->lightShader.id, 2, &count, shaders);
    myAssert(count == 2);

    glGetAttachedShaders(info->outlineShader.id, 2, &count, shaders);
    myAssert(count == 2);

    glGetAttachedShaders(info->textureShader.id, 2, &count, shaders);
    myAssert(count == 2);
}

internal u32 CreateTextureFromImage(const char *filename, GLenum wrapMode = GL_REPEAT)
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

    bool alpha = strstr(filename, "png") != NULL;
    GLenum pixelFormat = alpha ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, pixelFormat, width, height, 0, pixelFormat, GL_UNSIGNED_BYTE, textureData);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(textureData);

    return texture;
}

struct LoadedTextures
{
    Texture *textures;
    u32 numTextures;
};

internal void LoadTextures(Mesh *mesh, u64 num, aiMaterial *material, aiTextureType type, Arena *texturesArena,
                           LoadedTextures *loadedTextures)
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

internal Mesh ProcessMesh(aiMesh *mesh, const aiScene *scene, Arena *texturesArena, LoadedTextures *loadedTextures,
                          Arena *meshDataArena)
{
    Mesh result = {};

    result.verticesSize = mesh->mNumVertices * sizeof(Vertex);
    result.vertices = (Vertex *)ArenaPush(meshDataArena, result.verticesSize);
    myAssert(((u8 *)result.vertices + result.verticesSize) ==
             ((u8 *)meshDataArena->memory + meshDataArena->stackPointer));

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

        u32 *faceIndices = (u32 *)ArenaPush(meshDataArena, sizeof(u32) * face.mNumIndices);
        for (u32 j = 0; j < face.mNumIndices; j++)
        {
            faceIndices[j] = face.mIndices[j];
            indicesCount++;
        }
    }
    result.indicesSize = indicesCount * sizeof(u32);
    myAssert(((u8 *)result.indices + result.indicesSize) ==
             ((u8 *)meshDataArena->memory + meshDataArena->stackPointer));

    if (mesh->mMaterialIndex >= 0)
    {
        aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];

        u32 numDiffuse = material->GetTextureCount(aiTextureType_DIFFUSE);
        u32 numSpecular = material->GetTextureCount(aiTextureType_SPECULAR);
        u32 numTextures = numDiffuse + numSpecular;

        u64 texturesSize = sizeof(Texture) * numTextures;
        result.textures = (Texture *)ArenaPush(meshDataArena, texturesSize);

        LoadTextures(&result, numDiffuse, material, aiTextureType_DIFFUSE, texturesArena, loadedTextures);
        LoadTextures(&result, numSpecular, material, aiTextureType_SPECULAR, texturesArena, loadedTextures);
    }

    return result;
}

internal void ProcessNode(aiNode *node, const aiScene *scene, Mesh *meshes, u32 *meshCount, Arena *texturesArena,
                          LoadedTextures *loadedTextures, Arena *meshDataArena)
{
    for (u32 i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
        meshes[*meshCount] = ProcessMesh(mesh, scene, texturesArena, loadedTextures, meshDataArena);
        *meshCount += 1;
    }

    for (u32 i = 0; i < node->mNumChildren; i++)
    {
        ProcessNode(node->mChildren[i], scene, meshes, meshCount, texturesArena, loadedTextures, meshDataArena);
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

        glVertexAttribPointer(index, elemCount, GL_FLOAT, GL_FALSE, total * sizeof(float),
                              (void *)(accumulator * sizeof(float)));

        accumulator += elemCount;

        glEnableVertexAttribArray(index);
    }

    return vao;
}

internal Model LoadModel(const char *filename, s32 *elemCounts, u32 elemCountsSize, Arena *texturesArena,
                         Arena *meshDataArena)
{
    Model result;

    Assimp::Importer importer;
    char path[] = "backpack.obj";
    const aiScene *scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);
    myAssert(scene);
    Mesh *meshes = (Mesh *)ArenaPush(meshDataArena, 100 * sizeof(Mesh));
    u32 meshCount = 0;

    LoadedTextures loadedTextures = {};
    loadedTextures.textures = (Texture *)texturesArena->memory;
    ProcessNode(scene->mRootNode, scene, meshes, &meshCount, texturesArena, &loadedTextures, meshDataArena);

    u32 *meshVAOs = (u32 *)ArenaPush(meshDataArena, 100 * sizeof(u32));
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

bool LoadDrawingInfo(PersistentDrawingInfo *info, CameraInfo *cameraInfo)
{
    FILE *file;
    errno_t opened = fopen_s(&file, "save.bin", "rb");
    if (opened != 0)
    {
        return false;
    }

    fread(info, sizeof(PersistentDrawingInfo), 1, file);
    fread(cameraInfo, sizeof(CameraInfo), 1, file);

    fclose(file);

    return true;
}

GLenum CreateFramebuffer(s32 width, s32 height, u32 *fbo, u32 *quadTexture)
{
    glGenFramebuffers(1, fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, *fbo);

    glGenTextures(1, quadTexture);
    glBindTexture(GL_TEXTURE_2D, *quadTexture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *quadTexture, 0);

    u32 rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    return glCheckFramebufferStatus(GL_FRAMEBUFFER);
}

extern "C" __declspec(dllexport) bool InitializeDrawingInfo(HWND window, TransientDrawingInfo *transientInfo,
                                                            PersistentDrawingInfo *drawingInfo, CameraInfo *cameraInfo,
                                                            Arena *texturesArena, Arena *meshDataArena)
{
    u64 arenaSize = 100 * 1024 * 1024;

    if (!CreateShaderPrograms(transientInfo))
    {
        return false;
    }

    s32 elemCounts[] = {3, 3, 2};

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

    f32 quadVertices[] = {1.f,  1.f,  0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f,  -1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 0.f,
                          -1.f, -1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, -1.f, 1.f,  0.f, 0.f, 0.f, 1.f, 0.f, 1.f};

    u32 quadIndices[] = {0, 1, 3, 1, 2, 3};

    vaoInfo.vertices = quadVertices;
    vaoInfo.verticesSize = sizeof(quadVertices);
    vaoInfo.elemCounts = elemCounts;
    vaoInfo.elementCountsSize = myArraySize(elemCounts);
    vaoInfo.indices = quadIndices;
    vaoInfo.indicesSize = sizeof(quadIndices);

    u32 mainQuadVao = CreateVAO(&vaoInfo);

    f32 rearViewQuadVertices[] = {.5f,  1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, .5f,  .7f, 0.f, 0.f, 0.f, 1.f, 1.f, 0.f,
                                  -.5f, .7f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, -.5f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 1.f};

    u32 rearViewQuadIndices[] = {0, 1, 3, 1, 2, 3};

    vaoInfo.vertices = rearViewQuadVertices;
    vaoInfo.verticesSize = sizeof(rearViewQuadVertices);
    vaoInfo.elemCounts = elemCounts;
    vaoInfo.elementCountsSize = myArraySize(elemCounts);
    vaoInfo.indices = rearViewQuadIndices;
    vaoInfo.indicesSize = sizeof(rearViewQuadIndices);

    u32 rearViewQuadVao = CreateVAO(&vaoInfo);

    transientInfo->cubeVao = cubeVao;
    transientInfo->mainQuadVao = mainQuadVao;
    transientInfo->rearViewQuadVao = rearViewQuadVao;
    transientInfo->backpack =
        LoadModel("backpack.obj", elemCounts, myArraySize(elemCounts), texturesArena, meshDataArena);
    transientInfo->grassTexture = CreateTextureFromImage("grass.png", GL_CLAMP_TO_EDGE);
    transientInfo->windowTexture = CreateTextureFromImage("window.png", GL_CLAMP_TO_EDGE);

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
            pointLights[lightIndex].position = {x, y, z};

            u32 attIndex = rand() % myArraySize(globalAttenuationTable);
            // NOTE: constant-range point lights makes for easier visualization of light effects.
            pointLights[lightIndex].attIndex = 4; // clamp(attIndex, 2, 6)
        }

        for (u32 texCubeIndex = 0; texCubeIndex < NUM_OBJECTS; texCubeIndex++)
        {
            f32 x = (f32)(rand() % 10);
            x = (rand() > RAND_MAX / 2) ? x : -x;
            f32 y = (f32)(rand() % 10);
            y = (rand() > RAND_MAX / 2) ? y : -y;
            f32 z = (f32)(rand() % 10);
            z = (rand() > RAND_MAX / 2) ? z : -z;
            drawingInfo->texCubePos[texCubeIndex] = {x, y, z};
        }

        for (u32 windowIndex = 0; windowIndex < NUM_OBJECTS; windowIndex++)
        {
            f32 x = (f32)(rand() % 10);
            x = (rand() > RAND_MAX / 2) ? x : -x;
            f32 y = (f32)(rand() % 10);
            y = (rand() > RAND_MAX / 2) ? y : -y;
            f32 z = (f32)(rand() % 10);
            z = (rand() > RAND_MAX / 2) ? z : -z;
            drawingInfo->windowPos[windowIndex] = {x, y, z};
        }

        drawingInfo->dirLight.direction =
            glm::normalize(pointLights[NUM_POINTLIGHTS - 1].position - pointLights[0].position);
    }

    RECT clientRect;
    GetClientRect(window, &clientRect);
    s32 width = clientRect.right;
    s32 height = clientRect.bottom;

    GLenum mainFramebufferStatus = CreateFramebuffer(width, height, &transientInfo->mainFBO, &transientInfo->mainQuad);
    myAssert(mainFramebufferStatus == GL_FRAMEBUFFER_COMPLETE);

    GLenum rearViewFramebufferStatus =
        CreateFramebuffer(width, height, &transientInfo->rearViewFBO, &transientInfo->rearViewQuad);
    myAssert(rearViewFramebufferStatus == GL_FRAMEBUFFER_COMPLETE);

    u32 skyboxTexture;
    glGenTextures(1, &skyboxTexture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture);

    const char *skyboxImages[] = {"skybox/right.jpg",  "skybox/left.jpg",  "skybox/top.jpg",
                                  "skybox/bottom.jpg", "skybox/front.jpg", "skybox/back.jpg"};
    stbi_set_flip_vertically_on_load_thread(false);
    for (u32 i = 0; i < 6; i++)
    {
        s32 imageWidth;
        s32 imageHeight;
        s32 numChannels;
        u8 *data = stbi_load(skyboxImages[i], &imageWidth, &imageHeight, &numChannels, 0);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, imageWidth, imageHeight, 0, GL_RGB,
                     GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    transientInfo->skyboxTexture = skyboxTexture;

    glDepthFunc(GL_LEQUAL); // All skybox points are given a depth of 1.f.

    drawingInfo->initialized = true;

    cameraInfo->aspectRatio = (f32)width / (f32)height;
    
    glGenBuffers(1, &transientInfo->matricesUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, transientInfo->matricesUBO);
    glBufferData(GL_UNIFORM_BUFFER, 128, NULL, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, transientInfo->matricesUBO);

    return true;
}

internal glm::mat4 LookAt(CameraInfo *cameraInfo, glm::vec3 cameraTarget, glm::vec3 cameraUpVector,
                          float farPlaneDistance)
{
    glm::mat4 inverseTranslation = glm::mat4(1.f);
    inverseTranslation[3][0] = -cameraInfo->pos.x;
    inverseTranslation[3][1] = -cameraInfo->pos.y;
    inverseTranslation[3][2] = -cameraInfo->pos.z;

    glm::vec3 direction = glm::normalize(cameraInfo->pos - cameraTarget);
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
    glm::mat4 inverseScale = {
        1.f / (farPlaneDistance * (tanf(cameraInfo->fov / cameraInfo->aspectRatio * PI / 180.f) / 2.f)),
        0.f,
        0.f,
        0.f,
        0.f,
        1.f / (farPlaneDistance * (tanf(cameraInfo->fov * PI / 180.f) / 2.f)),
        0.f,
        0.f,
        0.f,
        0.f,
        1.f / farPlaneDistance,
        0.f,
        0.f,
        0.f,
        0.f,
        1.f};

    return inverseRotation * inverseTranslation;
}

extern "C" __declspec(dllexport) void SaveDrawingInfo(PersistentDrawingInfo *info, CameraInfo *cameraInfo)
{
    FILE *file;
    fopen_s(&file, "save.bin", "wb");

    fwrite(info, sizeof(PersistentDrawingInfo), 1, file);
    fwrite(cameraInfo, sizeof(CameraInfo), 1, file);

    fclose(file);
}

internal glm::mat4 GetCameraWorldRotation(CameraInfo *cameraInfo)
{
    // We are rotating in world space so this returns a world-space rotation.
    glm::vec3 cameraYawAxis = glm::vec3(0.f, 1.f, 0.f);
    glm::vec3 cameraPitchAxis = glm::vec3(1.f, 0.f, 0.f);

    glm::mat4 cameraYaw = glm::rotate(glm::mat4(1.f), cameraInfo->yaw, cameraYawAxis);
    glm::mat4 cameraPitch = glm::rotate(glm::mat4(1.f), cameraInfo->pitch, cameraPitchAxis);
    glm::mat4 cameraRotation = cameraYaw * cameraPitch;

    return cameraRotation;
}

internal glm::vec3 GetCameraRightVector(CameraInfo *cameraInfo)
{
    // World-space rotation * world-space axis -> world-space value.
    glm::vec4 cameraRightVec = GetCameraWorldRotation(cameraInfo) * glm::vec4(1.f, 0.f, 0.f, 0.f);

    return glm::vec3(cameraRightVec);
}

internal glm::vec3 GetCameraForwardVector(CameraInfo *cameraInfo)
{
    glm::vec4 cameraForwardVec = GetCameraWorldRotation(cameraInfo) * glm::vec4(0.f, 0.f, -1.f, 0.f);

    return glm::vec3(cameraForwardVec);
}

extern "C" __declspec(dllexport) void ProvideCameraVectors(CameraInfo *cameraInfo)
{
    cameraInfo->forwardVector = GetCameraForwardVector(cameraInfo);
    cameraInfo->rightVector = GetCameraRightVector(cameraInfo);
};

internal bool HasNewVersion(const char *filename, FILETIME *lastFileTime)
{
    HANDLE file = CreateFileA(filename, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    FILETIME fileTime = {};
    GetFileTime(file, 0, 0, &fileTime);
    CloseHandle(file);

    bool returnVal = (CompareFileTime(lastFileTime, &fileTime) != 0);
    *lastFileTime = fileTime;
    return returnVal;
}

// TODO: develop a better way to do this that doesn't involve adding another 4 lines
// every time I add another shader program.
internal void CheckForNewShaders(TransientDrawingInfo *info)
{
    if (HasNewVersion(info->objectShader.vertexShaderFilename, &info->objectShader.vertexShaderTime) ||
        HasNewVersion(info->objectShader.fragmentShaderFilename, &info->objectShader.fragmentShaderTime) ||
        HasNewVersion(info->lightShader.vertexShaderFilename, &info->lightShader.vertexShaderTime) ||
        HasNewVersion(info->lightShader.fragmentShaderFilename, &info->lightShader.fragmentShaderTime) ||
        HasNewVersion(info->outlineShader.vertexShaderFilename, &info->outlineShader.vertexShaderTime) ||
        HasNewVersion(info->outlineShader.fragmentShaderFilename, &info->outlineShader.fragmentShaderTime) ||
        HasNewVersion(info->textureShader.vertexShaderFilename, &info->textureShader.vertexShaderTime) ||
        HasNewVersion(info->textureShader.fragmentShaderFilename, &info->textureShader.fragmentShaderTime) ||
        HasNewVersion(info->glassShader.vertexShaderFilename, &info->glassShader.vertexShaderTime) ||
        HasNewVersion(info->glassShader.fragmentShaderFilename, &info->glassShader.fragmentShaderTime) ||
        HasNewVersion(info->postProcessShader.vertexShaderFilename, &info->postProcessShader.vertexShaderTime) ||
        HasNewVersion(info->postProcessShader.fragmentShaderFilename, &info->postProcessShader.fragmentShaderTime) ||
        HasNewVersion(info->skyboxShader.vertexShaderFilename, &info->skyboxShader.vertexShaderTime) ||
        HasNewVersion(info->skyboxShader.fragmentShaderFilename, &info->skyboxShader.fragmentShaderTime))
    {
        CreateShaderPrograms(info);
    }
}

void RenderObject(u32 vao, u32 numIndices, glm::vec3 position, u32 shaderProgram, f32 yRot = 0.f, float scale = 1.f)
{
    glBindVertexArray(vao);

    // Model matrix: transforms vertices from local to world space.
    glm::mat4 modelMatrix = glm::mat4(1.f);
    modelMatrix = glm::translate(modelMatrix, position);
    modelMatrix = glm::rotate(modelMatrix, yRot, glm::vec3(0.f, 1.f, 0.f));
    modelMatrix = glm::scale(modelMatrix, glm::vec3(scale));
    glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(modelMatrix)));

    SetShaderUniformMat4(shaderProgram, "modelMatrix", &modelMatrix);
    SetShaderUniformMat3(shaderProgram, "normalMatrix", &normalMatrix);
    glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, 0);
}

void RenderWithLightShader(TransientDrawingInfo *transientInfo, PersistentDrawingInfo *persistentInfo)
{
    u32 shaderProgram = transientInfo->lightShader.id;
    glUseProgram(shaderProgram);

    glBindVertexArray(transientInfo->cubeVao);

    for (u32 lightIndex = 0; lightIndex < NUM_POINTLIGHTS; lightIndex++)
    {
        PointLight *curLight = &persistentInfo->pointLights[lightIndex];

        glEnable(GL_STENCIL_TEST);
        glStencilMask(0xff);
        glStencilFunc(GL_ALWAYS, 1, 0xff);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

        SetShaderUniformVec3(shaderProgram, "lightColor", curLight->diffuse);
        RenderObject(transientInfo->cubeVao, 36, curLight->position, shaderProgram);

        glStencilMask(0x00);
        glStencilFunc(GL_NOTEQUAL, 1, 0xff);

        glm::vec4 stencilColor = glm::vec4(0.f, 0.f, 1.f, 1.f);
        SetShaderUniformVec3(shaderProgram, "lightColor", stencilColor);
        RenderObject(transientInfo->cubeVao, 36, curLight->position, shaderProgram, 0.f, 1.1f);

        glDisable(GL_STENCIL_TEST);
    }
}

void DrawScene(CameraInfo *cameraInfo, TransientDrawingInfo *transientInfo, PersistentDrawingInfo *persistentInfo,
               u32 fbo, u32 quadTexture, HWND window, Arena *listArena, Arena *tempArena, bool dynamicEnvPass = false);

void RenderModel(Model *model, u32 shaderProgram, u32 skyboxTexture)
{
    for (u32 i = 0; i < model->meshCount; i++)
    {
        glUseProgram(shaderProgram);

        glBindVertexArray(model->vaos[i]);

        Mesh *mesh = &model->meshes[i];
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

        // Skybox contribution.
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture);

        // Model matrix: transforms vertices from local to world space.
        glm::mat4 modelMatrix = glm::mat4(1.f);
        modelMatrix = glm::translate(modelMatrix, glm::vec3(0.f));

        glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(modelMatrix)));

        // glEnable(GL_STENCIL_TEST);
        // glStencilMask(0xff);
        // glStencilFunc(GL_ALWAYS, 1, 0xff);
        // glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

        SetShaderUniformMat4(shaderProgram, "modelMatrix", &modelMatrix);
        SetShaderUniformMat3(shaderProgram, "normalMatrix", &normalMatrix);
        glDrawElements(GL_TRIANGLES, mesh->verticesSize / sizeof(Vertex), GL_UNSIGNED_INT, 0);

        // glStencilMask(0x00);

        // shaderProgram = transientInfo->outlineShaderProgram;
        // glUseProgram(shaderProgram);

        // glDisable(GL_DEPTH_TEST);
        // glStencilFunc(GL_NOTEQUAL, 1, 0xff);

        // glm::vec4 stencilColor = glm::vec4(0.f, 0.f, 1.f, 1.f);
        // SetShaderUniformVec4(shaderProgram, "color", stencilColor);
        // glm::mat4 stencilModelMatrix = glm::scale(modelMatrix, glm::vec3(1.02f));
        // SetShaderUniformMat4(shaderProgram, "modelMatrix", &stencilModelMatrix);
        // SetShaderUniformMat3(shaderProgram, "normalMatrix", &normalMatrix);
        // SetShaderUniformMat4(shaderProgram, "viewMatrix", &viewMatrix);
        // SetShaderUniformMat4(shaderProgram, "projectionMatrix", &projectionMatrix);
        // glDrawElements(GL_TRIANGLES, mesh->verticesSize / sizeof(Vertex), GL_UNSIGNED_INT, 0);

        // glDisable(GL_STENCIL_TEST);
        // glEnable(GL_DEPTH_TEST);
    }
}

void RenderWithObjectShader(CameraInfo *cameraInfo, TransientDrawingInfo *transientInfo,
                            PersistentDrawingInfo *persistentInfo, HWND window, Arena *listArena,
                            Arena *tempArena, bool dynamicEnvPass = false)
{
    local_persist EnvironmentMap dynamicEnvMap;
    // Dynamic environment mapping.
    {
        s32 width = 1024;
        s32 height = 1024;
        
        if (!dynamicEnvMap.initialized)
        {            
            for (u32 i = 0; i < 6; i++)
            {
                GLenum framebufferStatus =
                    CreateFramebuffer(width, height, &dynamicEnvMap.FBOs[i], &dynamicEnvMap.quads[i]);
                myAssert(framebufferStatus == GL_FRAMEBUFFER_COMPLETE);
            }
            
            glGenTextures(1, &dynamicEnvMap.skyboxTexture);
            glBindTexture(GL_TEXTURE_CUBE_MAP, dynamicEnvMap.skyboxTexture);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            
            dynamicEnvMap.initialized = true;
        }
        
        if (!dynamicEnvPass)
        {
            s32 savedFBO;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &savedFBO);
            glm::mat4 savedViewMatrix;
            glGetNamedBufferSubData(transientInfo->matricesUBO, 0, 64, &savedViewMatrix);
            glm::mat4 savedProjectionMatrix;
            glGetNamedBufferSubData(transientInfo->matricesUBO, 64, 64, &savedProjectionMatrix);
        
            float orientations[6][2] =
            {
                {PI, 0.f},
                {0.f, 0.f},
                {0.f, PI / 2},
                {0.f, -PI / 2},
                {PI / 2.f, 0.f},
                {-PI / 2.f, 0.f},
            };
            
            CameraInfo centralCamera = {};
            centralCamera.aspectRatio = cameraInfo->aspectRatio;
            centralCamera.fov = cameraInfo->fov;
            
            for (u32 i = 0; i < 6; i++)
            {
                centralCamera.yaw = orientations[i][0];
                centralCamera.pitch = orientations[i][1];
                centralCamera.forwardVector = GetCameraForwardVector(&centralCamera);
                centralCamera.rightVector = GetCameraRightVector(&centralCamera);
                DrawScene(&centralCamera, transientInfo, persistentInfo, dynamicEnvMap.FBOs[i], dynamicEnvMap.quads[i],
                          window, listArena, tempArena, true);
            }
            
            u8 *data = (u8 *)ArenaPush(tempArena, 1920 * 1080 * 4);
            for (u32 i = 0; i < 6; i++)
            {
                glBindTexture(GL_TEXTURE_2D, dynamicEnvMap.quads[i]);
                glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
                
                glBindTexture(GL_TEXTURE_CUBE_MAP, dynamicEnvMap.skyboxTexture);
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB,
                             GL_UNSIGNED_BYTE, data);
            }
            ArenaPop(tempArena, 1920 * 1080 * 4);
            
            glBindFramebuffer(GL_FRAMEBUFFER, savedFBO);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, 64, &savedViewMatrix);
            glBufferSubData(GL_UNIFORM_BUFFER, 64, 64, &savedProjectionMatrix);
        }
        else
        {
            return;
        }
    }
    
    u32 shaderProgram = transientInfo->objectShader.id;
    glUseProgram(shaderProgram);

    SetShaderUniformFloat(shaderProgram, "material.shininess", 32.f);

    SetShaderUniformVec3(shaderProgram, "dirLight.direction", persistentInfo->dirLight.direction);
    SetShaderUniformVec3(shaderProgram, "dirLight.ambient", persistentInfo->dirLight.ambient);
    SetShaderUniformVec3(shaderProgram, "dirLight.diffuse", persistentInfo->dirLight.diffuse);
    SetShaderUniformVec3(shaderProgram, "dirLight.specular", persistentInfo->dirLight.specular);

    for (u32 lightIndex = 0; lightIndex < NUM_POINTLIGHTS; lightIndex++)
    {
        PointLight *lights = persistentInfo->pointLights;
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

    SetShaderUniformVec3(shaderProgram, "spotLight.position", cameraInfo->pos);
    SetShaderUniformVec3(shaderProgram, "spotLight.direction", GetCameraForwardVector(cameraInfo));
    SetShaderUniformFloat(shaderProgram, "spotLight.innerCutoff", cosf(persistentInfo->spotLight.innerCutoff));
    SetShaderUniformFloat(shaderProgram, "spotLight.outerCutoff", cosf(persistentInfo->spotLight.outerCutoff));
    SetShaderUniformVec3(shaderProgram, "spotLight.ambient", persistentInfo->spotLight.ambient);
    SetShaderUniformVec3(shaderProgram, "spotLight.diffuse", persistentInfo->spotLight.diffuse);
    SetShaderUniformVec3(shaderProgram, "spotLight.specular", persistentInfo->spotLight.specular);

    SetShaderUniformVec3(shaderProgram, "cameraPos", cameraInfo->pos);

    RenderModel(&transientInfo->backpack, shaderProgram, dynamicEnvMap.skyboxTexture);
}

void RenderWithTextureShader(CameraInfo *cameraInfo, TransientDrawingInfo *transientInfo,
                             PersistentDrawingInfo *persistentInfo)

{
    u32 shaderProgram = transientInfo->textureShader.id;
    glUseProgram(shaderProgram);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, transientInfo->windowTexture);

    SetShaderUniformVec3(shaderProgram, "cameraPos", cameraInfo->pos);

    glEnable(GL_CULL_FACE);
    for (u32 i = 0; i < NUM_OBJECTS; i++)
    {
        glm::vec3 position = persistentInfo->texCubePos[i];
        
        RenderObject(transientInfo->cubeVao, 36, position, shaderProgram);
    }
    glDisable(GL_CULL_FACE);
}

void RenderWithGlassShader(CameraInfo *cameraInfo, TransientDrawingInfo *transientInfo,
                           PersistentDrawingInfo *persistentInfo, Arena *listArena,
                           Arena *tempArena)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    u32 shaderProgram = transientInfo->glassShader.id;
    glUseProgram(shaderProgram);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, transientInfo->windowTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, transientInfo->skyboxTexture);

    SkipList list = CreateNewList(listArena);
    for (u32 i = 0; i < NUM_OBJECTS; i++)
    {
        f32 dist = glm::distance(cameraInfo->pos, persistentInfo->windowPos[i]);
        Insert(&list, dist, persistentInfo->windowPos[i], listArena, tempArena);
    }

    for (u32 i = 0; i < NUM_OBJECTS; i++)
    {
        glm::vec3 pos = GetValue(&list, i);
        
        RenderObject(transientInfo->mainQuadVao, 6, pos, shaderProgram, cameraInfo->yaw);
    }
    ArenaClear(listArena);
    memset(listArena->memory, 0, listArena->size);
    ArenaClear(tempArena);

    glDisable(GL_BLEND);
}

void DrawScene(CameraInfo *cameraInfo, TransientDrawingInfo *transientInfo, PersistentDrawingInfo *persistentInfo,
               u32 fbo, u32 quadTexture, HWND window, Arena *listArena, Arena *tempArena, bool dynamicEnvPass)
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    float *cc = persistentInfo->clearColor;
    glClearColor(cc[0], cc[1], cc[2], cc[3]);
    glStencilMask(0xff);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glStencilMask(0x00);

    glm::vec3 cameraForwardVec = glm::normalize(GetCameraForwardVector(cameraInfo));
    glm::vec3 cameraTarget = cameraInfo->pos + cameraForwardVec;

    glm::vec3 cameraDirection = glm::normalize(cameraInfo->pos - cameraTarget);

    glm::vec3 upVector = glm::vec3(0.f, 1.f, 0.f);
    glm::vec3 cameraRightVec = glm::normalize(glm::cross(upVector, cameraDirection));
    glm::vec3 cameraUpVec = glm::normalize(glm::cross(cameraDirection, cameraRightVec));

    float farPlaneDistance = 100.f;

    glm::mat4 viewMatrix = LookAt(cameraInfo, cameraTarget, cameraUpVec, farPlaneDistance);

    // Projection matrix: transforms vertices from view space to clip space.
    glm::mat4 projectionMatrix =
        glm::perspective(glm::radians(cameraInfo->fov), cameraInfo->aspectRatio, .1f, farPlaneDistance);
    
    glBindBuffer(GL_UNIFORM_BUFFER, transientInfo->matricesUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, 64, &viewMatrix);
    glBufferSubData(GL_UNIFORM_BUFFER, 64, 64, &projectionMatrix);

    // TODO: fix effect of outlining on meshes that appear between the camera and the outlined object.

    // Point lights.
    RenderWithLightShader(transientInfo, persistentInfo);

    // Objects.
    RenderWithObjectShader(cameraInfo, transientInfo, persistentInfo, window, listArena, tempArena, dynamicEnvPass);

    // Textured cubes.
    RenderWithTextureShader(cameraInfo, transientInfo, persistentInfo);

    // Windows.
    RenderWithGlassShader(cameraInfo, transientInfo, persistentInfo, listArena, tempArena);

    // Skybox.
    {
        u32 shaderProgram = transientInfo->skyboxShader.id;
        glUseProgram(shaderProgram);

        glm::mat4 skyboxViewMatrix = glm::mat4(glm::mat3(viewMatrix));
        glBufferSubData(GL_UNIFORM_BUFFER, 0, 64, &skyboxViewMatrix);

        glBindVertexArray(transientInfo->cubeVao);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, transientInfo->skyboxTexture);

        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    }
}

void DrawDebugWindow(CameraInfo *cameraInfo, PersistentDrawingInfo *persistentInfo)
{
    u32 id = 0;
    ImGui::Begin("Debug Window");

    ImGui::SliderFloat3("Camera position", glm::value_ptr(cameraInfo->pos), -150.f, 150.f);
    ImGui::SliderFloat2("Camera rotation", &cameraInfo->yaw, -PI, PI);
    if (ImGui::Button("Reset camera"))
    {
        cameraInfo->pos = glm::vec3(0.f);
        cameraInfo->yaw = 0.f;
        cameraInfo->pitch = 0.f;
    }

    ImGui::Text("Blending: %s", glIsEnabled(GL_BLEND) ? "enabled" : "disabled");
    ImGui::SameLine();
    if (ImGui::Button("Toggle blending"))
    {
        if (glIsEnabled(GL_BLEND))
        {
            glDisable(GL_BLEND);
        }
        else
        {
            glEnable(GL_BLEND);
        }
    }

    char depthTestFuncStr[16];
    s32 depthFunc;
    glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
    PrintDepthTestFunc(depthFunc, depthTestFuncStr, sizeof(depthTestFuncStr));
    ImGui::Text("Depth-test function (press U/I to change): %s", depthTestFuncStr);

    ImGui::Separator();

    ImGui::SliderFloat4("Clear color", persistentInfo->clearColor, 0.f, 1.f);

    if (ImGui::CollapsingHeader("Directional light"))
    {
        ImGui::PushID(id++);
        ImGui::SliderFloat3("Direction", glm::value_ptr(persistentInfo->dirLight.direction), -10.f, 10.f);
        ImGui::SliderFloat3("Ambient", glm::value_ptr(persistentInfo->dirLight.ambient), 0.f, 1.f);
        ImGui::SliderFloat3("Diffuse", glm::value_ptr(persistentInfo->dirLight.diffuse), 0.f, 1.f);
        ImGui::SliderFloat3("Specular", glm::value_ptr(persistentInfo->dirLight.specular), 0.f, 1.f);
        ImGui::PopID();
    }

    if (ImGui::CollapsingHeader("Point lights"))
    {
        PointLight *lights = persistentInfo->pointLights;
        for (u32 index = 0; index < NUM_POINTLIGHTS; index++)
        {
            ImGui::PushID(id++);
            char treeName[32];
            sprintf_s(treeName, "Light #%i", index);
            if (ImGui::TreeNode(treeName))
            {
                ImGui::SliderFloat3("Position", glm::value_ptr(lights[index].position), -10.f, 10.f);
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
        ImGui::SliderFloat3("Position", glm::value_ptr(persistentInfo->spotLight.position), -10.f, 10.f);
        ImGui::SliderFloat3("Direction", glm::value_ptr(persistentInfo->spotLight.direction), -10.f, 10.f);
        ImGui::SliderFloat3("Ambient", glm::value_ptr(persistentInfo->spotLight.ambient), 0.f, 1.f);
        ImGui::SliderFloat3("Diffuse", glm::value_ptr(persistentInfo->spotLight.diffuse), 0.f, 1.f);
        ImGui::SliderFloat3("Specular", glm::value_ptr(persistentInfo->spotLight.specular), 0.f, 1.f);
        ImGui::SliderFloat("Inner cutoff", &persistentInfo->spotLight.innerCutoff, 0.f, PI / 2.f);
        ImGui::SliderFloat("Outer cutoff", &persistentInfo->spotLight.outerCutoff, 0.f, PI / 2.f);
        ImGui::PopID();
    }

    if (ImGui::CollapsingHeader("Windows"))
    {
        glm::vec3 *windows = persistentInfo->windowPos;
        for (u32 index = 0; index < NUM_OBJECTS; index++)
        {
            ImGui::PushID(id++);
            char treeName[32];
            sprintf_s(treeName, "Window #%i", index);
            if (ImGui::TreeNode(treeName))
            {
                ImGui::SliderFloat3("Position", glm::value_ptr(windows[index]), -10.f, 10.f);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }

    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

extern "C" __declspec(dllexport) void DrawWindow(HWND window, HDC hdc, bool *running,
                                                 TransientDrawingInfo *transientInfo,
                                                 PersistentDrawingInfo *persistentInfo, CameraInfo *cameraInfo,
                                                 Arena *listArena, Arena *tempArena)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (!persistentInfo->initialized)
    {
        return;
    }

    CheckForNewShaders(transientInfo);

    // Main pass.
    DrawScene(cameraInfo, transientInfo, persistentInfo, transientInfo->mainFBO, transientInfo->mainQuad,
              window, listArena, tempArena);

    CameraInfo rearViewCamera = *cameraInfo;
    rearViewCamera.yaw += PI;
    rearViewCamera.pitch *= -1.f;
    rearViewCamera.forwardVector = GetCameraForwardVector(&rearViewCamera);
    rearViewCamera.rightVector = GetCameraRightVector(&rearViewCamera);

    // Rear-view pass.
    DrawScene(&rearViewCamera, transientInfo, persistentInfo, transientInfo->rearViewFBO, transientInfo->rearViewQuad,
              window, listArena, tempArena);
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(1.f, 1.f, 1.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Main quad.
    {
        u32 shaderProgram = transientInfo->postProcessShader.id;
        glUseProgram(shaderProgram);

        glDisable(GL_DEPTH_TEST);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, transientInfo->mainQuad);

        glm::mat4 identity = glm::mat4(1.f);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, 64, &identity);
        glBufferSubData(GL_UNIFORM_BUFFER, 64, 64, &identity);

        glBindVertexArray(transientInfo->mainQuadVao);

        // Model matrix: transforms vertices from local to world space.
        glm::mat4 modelMatrix = glm::mat4(1.f);
        modelMatrix = glm::translate(modelMatrix, glm::vec3(0.f));

        SetShaderUniformMat4(shaderProgram, "modelMatrix", &modelMatrix);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glEnable(GL_DEPTH_TEST);
    }

    // Rear-view quad.
    {
        u32 shaderProgram = transientInfo->postProcessShader.id;
        glUseProgram(shaderProgram);

        glDisable(GL_DEPTH_TEST);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, transientInfo->rearViewQuad);

        glm::mat4 identity = glm::mat4(1.f);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, 64, &identity);
        glBufferSubData(GL_UNIFORM_BUFFER, 64, 64, &identity);

        glBindVertexArray(transientInfo->rearViewQuadVao);

        // Model matrix: transforms vertices from local to world space.
        glm::mat4 modelMatrix = glm::mat4(1.f);
        modelMatrix = glm::translate(modelMatrix, glm::vec3(0.f));

        SetShaderUniformMat4(shaderProgram, "modelMatrix", &modelMatrix);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glEnable(GL_DEPTH_TEST);
    }

    DrawDebugWindow(cameraInfo, persistentInfo);

    if (!SwapBuffers(hdc))
    {
        if (MessageBoxW(window, L"Failed to swap buffers", L"OpenGL error", MB_OK) == S_OK)
        {
            *running = false;
        }
    }
}

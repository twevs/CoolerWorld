#include "common.h"
#include "skiplist.h"

#define NUM_ASTEROIDS 100000

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

    bool hasGeometryShader = strlen(program->geometryShaderFilename) > 0;
    u32 geometryShaderID = 0;
    if (hasGeometryShader && !CompileShader(&geometryShaderID, GL_GEOMETRY_SHADER, program->geometryShaderFilename))
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
    if (hasGeometryShader)
    {
        glAttachShader(*programID, geometryShaderID);
    }
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

internal void SetShaderUniformSampler(u32 shaderProgram, const char *uniformName, s32 slot)
{
    glUniform1i(glGetUniformLocation(shaderProgram, uniformName), slot);
}

internal void SetShaderUniformInt(u32 shaderProgram, const char *uniformName, s32 value)
{
    glUniform1i(glGetUniformLocation(shaderProgram, uniformName), value);
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
                                  const char *fragmentShaderFilename, const char *geometryShaderFilename = "")
{
    ShaderProgram newShaderProgram = {};
    strcpy_s(newShaderProgram.vertexShaderFilename, vertexShaderFilename);
    strcpy_s(newShaderProgram.geometryShaderFilename, geometryShaderFilename);
    strcpy_s(newShaderProgram.fragmentShaderFilename, fragmentShaderFilename);
    newShaderProgram.vertexShaderTime = GetFileTime(vertexShaderFilename);
    newShaderProgram.geometryShaderTime = GetFileTime(geometryShaderFilename);
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
    if (!CreateShaderProgram(&info->instancedObjectShader, info->instancedObjectShader.id, "instanced.vs",
                             "fragment_shader.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->colorShader, info->colorShader.id, "vertex_shader.vs", "color.fs"))
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
    if (!CreateShaderProgram(&info->geometryShader, info->geometryShader.id, "vertex_shader_geometry.vs", "color.fs",
                             "vis_normals.gs"))
    {
        return false;
    }

    glUseProgram(info->objectShader.id);
    SetShaderUniformSampler(info->objectShader.id, "material.diffuse", 0);
    SetShaderUniformSampler(info->objectShader.id, "material.specular", 1);
    SetShaderUniformSampler(info->objectShader.id, "skybox", 2);
    glUseProgram(info->instancedObjectShader.id);
    SetShaderUniformSampler(info->instancedObjectShader.id, "material.diffuse", 0);
    SetShaderUniformSampler(info->instancedObjectShader.id, "material.specular", 1);
    SetShaderUniformSampler(info->instancedObjectShader.id, "skybox", 2);

    glUseProgram(info->textureShader.id);
    SetShaderUniformSampler(info->textureShader.id, "tex", 0);

    glUseProgram(info->glassShader.id);
    SetShaderUniformSampler(info->glassShader.id, "tex", 0);
    SetShaderUniformSampler(info->glassShader.id, "skybox", 1);

    glUseProgram(info->skyboxShader.id);
    SetShaderUniformSampler(info->skyboxShader.id, "skybox", 0);

    return true;
}

/*
internal bool ReloadShaderPrograms(TransientDrawingInfo *info)
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
*/

internal u32 CreateTextureFromImage(const char *filename, bool sRGB, GLenum wrapMode = GL_REPEAT)
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
    if (sRGB)
    {
        GLenum internalFormat = alpha ? GL_SRGB_ALPHA : GL_SRGB;
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, pixelFormat, GL_UNSIGNED_BYTE, textureData);
    }
    else
    {
        glTexImage2D(GL_TEXTURE_2D, 0, pixelFormat, width, height, 0, pixelFormat, GL_UNSIGNED_BYTE, textureData);
    }
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
            texture->id = CreateTextureFromImage(path.C_Str(), type == aiTextureType_DIFFUSE);
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
    const aiScene *scene = importer.ReadFile(filename, aiProcess_Triangulate | aiProcess_FlipUVs);
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

internal bool LoadDrawingInfo(TransientDrawingInfo *transientInfo, PersistentDrawingInfo *info, CameraInfo *cameraInfo)
{
    FILE *file;
    errno_t opened = fopen_s(&file, "save.bin", "rb");
    if (opened != 0)
    {
        return false;
    }

    fread(info, sizeof(PersistentDrawingInfo), 1, file);
    transientInfo->numObjects = info->numObjects;
    for (u32 i = 0; i < transientInfo->numObjects; i++)
    {
        transientInfo->objects[i].position = info->objectPositions[i];
    }
    transientInfo->numModels = info->numModels;
    for (u32 i = 0; i < transientInfo->numModels; i++)
    {
        transientInfo->models[i].position = info->modelPositions[i];
    }
    fread(cameraInfo, sizeof(CameraInfo), 1, file);

    fclose(file);

    return true;
}

internal GLenum CreateMultisampledFramebuffer(s32 width, s32 height, u32 *fbo, u32 *quadTexture, u32 *rbo,
                                              s32 numSamples = 4)
{
    glGenFramebuffers(1, fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, *fbo);

    glGenTextures(1, quadTexture);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, *quadTexture);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, numSamples, GL_RGB, width, height, GL_TRUE);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, *quadTexture, 0);

    glGenRenderbuffers(1, rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, *rbo);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, numSamples, GL_DEPTH24_STENCIL8, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, *rbo);

    return glCheckFramebufferStatus(GL_FRAMEBUFFER);
}

internal GLenum CreateFramebuffer(s32 width, s32 height, u32 *fbo, u32 *quadTexture, u32 *rbo, bool depthMap = false)
{
    glGenFramebuffers(1, fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, *fbo);

    glGenTextures(1, quadTexture);
    glBindTexture(GL_TEXTURE_2D, *quadTexture);
    GLenum format = depthMap ? GL_DEPTH_COMPONENT : GL_RGB;
    GLenum type = depthMap ? GL_FLOAT : GL_UNSIGNED_BYTE;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, type, NULL);
    GLint filteringMethod = depthMap ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filteringMethod);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filteringMethod);
    if (depthMap)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    GLenum attachment = depthMap ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0;
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, *quadTexture, 0);
    if (depthMap)
    {
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }
    else
    {
        glGenRenderbuffers(1, rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, *rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, *rbo);
    }

    return glCheckFramebufferStatus(GL_FRAMEBUFFER);
}

// Returns the size of the previous buffer.
internal u32 AppendToVAO(u32 vao, void *data, u32 dataSize)
{
    glBindVertexArray(vao);
    s32 bufferSize;
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &bufferSize);
    Arena *tempArena = AllocArena(bufferSize);
    void *savedBuffer = ArenaPush(tempArena, bufferSize);
    glGetBufferSubData(GL_ARRAY_BUFFER, 0, bufferSize, savedBuffer);
    glBufferData(GL_ARRAY_BUFFER, bufferSize + dataSize, NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, bufferSize, savedBuffer);
    glBufferSubData(GL_ARRAY_BUFFER, bufferSize, dataSize, data);
    FreeArena(tempArena);
    return bufferSize;
}

internal f32 CreateRandomNumber(f32 min, f32 max)
{
    f32 midpoint = (max + min) / 2.f;
    f32 delta = midpoint - min;
    f32 x = (f32)(rand() % 10);
    x = (rand() > RAND_MAX / 2) ? x : -x;
    return midpoint + delta * (x / 10.f);
}

internal glm::vec3 CreateRandomVec3()
{
    f32 x = (f32)(rand() % 10);
    x = (rand() > RAND_MAX / 2) ? x : -x;
    f32 y = (f32)(rand() % 10);
    y = (rand() > RAND_MAX / 2) ? y : -y;
    f32 z = (f32)(rand() % 10);
    z = (rand() > RAND_MAX / 2) ? z : -z;

    return {x, y, z};
}

u32 CreateVAO(f32 *vertices, u32 verticesSize, s32 *elemCounts, u32 elemCountsSize, u32 *indices, u32 indicesSize)
{
    VaoInformation vaoInfo = {};
    vaoInfo.vertices = vertices;
    vaoInfo.verticesSize = verticesSize;
    vaoInfo.elemCounts = elemCounts;
    vaoInfo.elementCountsSize = elemCountsSize;
    vaoInfo.indices = indices;
    vaoInfo.indicesSize = indicesSize;

    return CreateVAO(&vaoInfo);
}

void SetUpAsteroids(Model *asteroid)
{
    u32 modelMatricesSize = NUM_ASTEROIDS * sizeof(glm::mat4);
    u32 radiiSize = NUM_ASTEROIDS * sizeof(f32);
    u32 yValuesSize = NUM_ASTEROIDS * sizeof(f32);

    Arena *tempArena = AllocArena(modelMatricesSize + radiiSize + yValuesSize);
    glm::mat4 *modelMatrices = (glm::mat4 *)ArenaPush(tempArena, modelMatricesSize);
    f32 *radii = (f32 *)ArenaPush(tempArena, radiiSize);
    f32 *yValues = (f32 *)ArenaPush(tempArena, yValuesSize);
    for (u32 i = 0; i < NUM_ASTEROIDS; i++)
    {
        float radius = CreateRandomNumber(.8f, 1.2f) * 50.f;
        radii[i] = radius;
        float y = CreateRandomNumber(-5.f, 5.f);
        yValues[i] = y;

        glm::mat4 modelMatrix = glm::mat4(1.f);
        f32 rotAngle = CreateRandomNumber(0, PI);
        modelMatrix = glm::rotate(modelMatrix, rotAngle, CreateRandomVec3());
        f32 scale = CreateRandomNumber(.01f, .2f);
        modelMatrix = glm::scale(modelMatrix, glm::vec3(scale));

        modelMatrices[i] = modelMatrix;
    }

    // NOTE: I originally used AppendToVAO() as I did not know that a VAO could have attribute pointers
    // into several VBOs. Out of curiosity, I profiled both methods and there is no performance
    // difference.
    u32 matricesVBO[3];
    glGenBuffers(3, matricesVBO);
    glBindBuffer(GL_ARRAY_BUFFER, matricesVBO[0]);
    glBufferData(GL_ARRAY_BUFFER, modelMatricesSize, modelMatrices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, matricesVBO[1]);
    glBufferData(GL_ARRAY_BUFFER, radiiSize, radii, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, matricesVBO[2]);
    glBufferData(GL_ARRAY_BUFFER, yValuesSize, yValues, GL_STATIC_DRAW);

    for (u32 i = 0; i < asteroid->meshCount; i++)
    {
        u32 curVao = asteroid->vaos[i];
        glBindVertexArray(curVao);
        glBindBuffer(GL_ARRAY_BUFFER, matricesVBO[0]);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 16 * sizeof(f32), (void *)(u64)0);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 16 * sizeof(f32), (void *)(0 + 4 * sizeof(f32)));
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 16 * sizeof(f32), (void *)(0 + 8 * sizeof(f32)));
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, 16 * sizeof(f32), (void *)(0 + 12 * sizeof(f32)));
        glBindBuffer(GL_ARRAY_BUFFER, matricesVBO[1]);
        glVertexAttribPointer(7, 1, GL_FLOAT, GL_FALSE, sizeof(f32), (void *)0);
        glBindBuffer(GL_ARRAY_BUFFER, matricesVBO[2]);
        glVertexAttribPointer(8, 1, GL_FLOAT, GL_FALSE, sizeof(f32), (void *)0);
        glEnableVertexAttribArray(3);
        glEnableVertexAttribArray(4);
        glEnableVertexAttribArray(5);
        glEnableVertexAttribArray(6);
        glEnableVertexAttribArray(7);
        glEnableVertexAttribArray(8);
        glVertexAttribDivisor(3, 1);
        glVertexAttribDivisor(4, 1);
        glVertexAttribDivisor(5, 1);
        glVertexAttribDivisor(6, 1);
        glVertexAttribDivisor(7, 1);
        glVertexAttribDivisor(8, 1);
    }
    FreeArena(tempArena);
}

void CreateFramebuffers(HWND window, TransientDrawingInfo *transientInfo)
{
    RECT clientRect;
    GetClientRect(window, &clientRect);
    s32 width = clientRect.right;
    s32 height = clientRect.bottom;

    transientInfo->numSamples = 4;

    GLenum mainFramebufferStatus =
        CreateMultisampledFramebuffer(width, height, &transientInfo->mainFBO, &transientInfo->mainQuad,
                                      &transientInfo->mainRBO, transientInfo->numSamples);
    myAssert(mainFramebufferStatus == GL_FRAMEBUFFER_COMPLETE);

    GLenum rearViewFramebufferStatus = CreateFramebuffer(width, height, &transientInfo->rearViewFBO,
                                                         &transientInfo->rearViewQuad, &transientInfo->rearViewRBO);
    myAssert(rearViewFramebufferStatus == GL_FRAMEBUFFER_COMPLETE);

    GLenum postProcessingFramebufferStatus =
        CreateFramebuffer(width, height, &transientInfo->postProcessingFBO, &transientInfo->postProcessingQuad,
                          &transientInfo->postProcessingRBO);
    myAssert(postProcessingFramebufferStatus == GL_FRAMEBUFFER_COMPLETE);

    GLenum depthMapFramebufferStatus = CreateFramebuffer(
        width, height, &transientInfo->depthMapFBO, &transientInfo->depthMapQuad, &transientInfo->depthMapRBO, true);
    myAssert(depthMapFramebufferStatus == GL_FRAMEBUFFER_COMPLETE);
}

void CreateSkybox(TransientDrawingInfo *transientInfo)
{
    u32 skyboxTexture;
    glGenTextures(1, &skyboxTexture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture);

    const char *skyboxImages[] = {"space_skybox/right.png",  "space_skybox/left.png",  "space_skybox/top.png",
                                  "space_skybox/bottom.png", "space_skybox/front.png", "space_skybox/back.png"};
    stbi_set_flip_vertically_on_load_thread(false);
    for (u32 i = 0; i < 6; i++)
    {
        s32 imageWidth;
        s32 imageHeight;
        s32 numChannels;
        u8 *data = stbi_load(skyboxImages[i], &imageWidth, &imageHeight, &numChannels, 0);
        myAssert(data);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, imageWidth, imageHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    transientInfo->skyboxTexture = skyboxTexture;
}

Model *AddModel(const char *filename, TransientDrawingInfo *transientInfo, s32 *elemCounts, u32 elemCountsSize, Arena *texturesArena, Arena *meshDataArena)
{
    u32 modelIndex = transientInfo->numModels;
    transientInfo->models[modelIndex] = LoadModel(filename, elemCounts, elemCountsSize, texturesArena, meshDataArena);
    transientInfo->numModels++;
    myAssert(transientInfo->numModels <= MAX_MODELS);
    return &transientInfo->models[modelIndex];
}

Object *AddObject(TransientDrawingInfo *transientInfo, u32 vao, u32 numIndices, glm::vec3 position)
{
    u32 objectIndex = transientInfo->numObjects;
    transientInfo->objects[objectIndex] = {vao, numIndices, position};
    transientInfo->numObjects++;
    myAssert(transientInfo->numObjects <= MAX_OBJECTS);
    return &transientInfo->objects[objectIndex];
}

void AddModelToShaderPass(ShaderProgram *shader, Model *model)
{
    u32 modelIndex = shader->numModels;
    shader->shaderPassModels[modelIndex] = model;
    shader->numModels++;
    myAssert(shader->numModels <= MAX_MODELS);
}

void AddObjectToShaderPass(ShaderProgram *shader, Object *object)
{
    u32 objectIndex = shader->numObjects;
    shader->shaderPassObjects[objectIndex] = object;
    shader->numObjects++;
    myAssert(shader->numObjects <= MAX_OBJECTS);
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
    
    Model *backpack = AddModel("backpack.obj", transientInfo, elemCounts, myArraySize(elemCounts), texturesArena, meshDataArena);
    Model *planet = AddModel("planet.obj", transientInfo, elemCounts, myArraySize(elemCounts), texturesArena, meshDataArena);
    Model *rock = AddModel("rock.obj", transientInfo, elemCounts, myArraySize(elemCounts), texturesArena, meshDataArena);
    
    AddModelToShaderPass(&transientInfo->objectShader, backpack);
    AddModelToShaderPass(&transientInfo->geometryShader, backpack);
    
    FILE *rectFile;
    fopen_s(&rectFile, "rect.bin", "rb");
    // 4 vertices per face * 6 faces * (vec3 pos + vec3 normal + vec2 texCoords).
    f32 rectVertices[192];
    u32 rectIndices[36];
    fread(rectVertices, sizeof(f32), myArraySize(rectVertices), rectFile);
    fread(rectIndices, sizeof(u32), myArraySize(rectIndices), rectFile);
    fclose(rectFile);

    transientInfo->cubeVao = CreateVAO(rectVertices, sizeof(rectVertices), elemCounts, myArraySize(elemCounts),
                                       rectIndices, sizeof(rectIndices));

    f32 quadVertices[] = {1.f,  1.f,  0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f,  -1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 0.f,
                          -1.f, -1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, -1.f, 1.f,  0.f, 0.f, 0.f, 1.f, 0.f, 1.f};
    u32 quadIndices[] = {0, 1, 3, 1, 2, 3};
    u32 mainQuadVao = CreateVAO(quadVertices, sizeof(quadVertices), elemCounts, myArraySize(elemCounts), quadIndices,
                                sizeof(quadIndices));
    transientInfo->mainQuadVao = mainQuadVao;
    transientInfo->postProcessingQuadVao = mainQuadVao;
    transientInfo->depthMapQuadVao = mainQuadVao;

    f32 rearViewQuadVertices[] = {.5f,  1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, .5f,  .7f, 0.f, 0.f, 0.f, 1.f, 1.f, 0.f,
                                  -.5f, .7f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, -.5f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 1.f};
    u32 rearViewQuadIndices[] = {0, 1, 3, 1, 2, 3};
    transientInfo->rearViewQuadVao =
        CreateVAO(rearViewQuadVertices, sizeof(rearViewQuadVertices), elemCounts, myArraySize(elemCounts),
                  rearViewQuadIndices, sizeof(rearViewQuadIndices));

    srand((u32)Win32GetWallClock());

    SetUpAsteroids(&transientInfo->models[2]);

    transientInfo->grassTexture = CreateTextureFromImage("grass.png", true, GL_CLAMP_TO_EDGE);
    transientInfo->windowTexture = CreateTextureFromImage("window.png", true, GL_CLAMP_TO_EDGE);

    if (!LoadDrawingInfo(transientInfo, drawingInfo, cameraInfo))
    {
        PointLight *pointLights = drawingInfo->pointLights;

        // TODO: account for in refactor.
        for (u32 lightIndex = 0; lightIndex < NUM_POINTLIGHTS; lightIndex++)
        {
            pointLights[lightIndex].position = CreateRandomVec3();

            u32 attIndex = rand() % myArraySize(globalAttenuationTable);
            // NOTE: constant-range point lights makes for easier visualization of light effects.
            pointLights[lightIndex].attIndex = 4; // clamp(attIndex, 2, 6)
        }

        // TODO: account for in refactor.
        for (u32 texCubeIndex = 0; texCubeIndex < NUM_OBJECTS; texCubeIndex++)
        {
            Object *texCube = AddObject(transientInfo, transientInfo->cubeVao, 36, CreateRandomVec3());
            AddObjectToShaderPass(&transientInfo->textureShader, texCube);
        }

        for (u32 windowIndex = 0; windowIndex < NUM_OBJECTS; windowIndex++)
        {
            Object *curWindow = AddObject(transientInfo, transientInfo->mainQuadVao, 6, CreateRandomVec3());
            AddObjectToShaderPass(&transientInfo->textureShader, curWindow);
        }

        drawingInfo->dirLight.direction =
            glm::normalize(pointLights[NUM_POINTLIGHTS - 1].position - pointLights[0].position);
    }

    CreateFramebuffers(window, transientInfo);

    CreateSkybox(transientInfo);

    glDepthFunc(GL_LEQUAL); // All skybox points are given a depth of 1.f.

    drawingInfo->initialized = true;

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

extern "C" __declspec(dllexport) void SaveDrawingInfo(TransientDrawingInfo *transientInfo, PersistentDrawingInfo *info, CameraInfo *cameraInfo)
{
    FILE *file;
    fopen_s(&file, "save.bin", "wb");

    info->numObjects = transientInfo->numObjects;
    for (u32 i = 0; i < transientInfo->numObjects; i++)
    {
         info->objectPositions[i] = transientInfo->objects[i].position;
    }
    for (u32 i = 0; i < transientInfo->numModels; i++)
    {
         info->modelPositions[i] = transientInfo->models[i].position;
    }
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

internal bool HasNewVersion(ShaderProgram *program)
{
    bool hasGeometryShader = (strlen(program->geometryShaderFilename) > 0);
    return HasNewVersion(program->vertexShaderFilename, &program->vertexShaderTime) ||
           (hasGeometryShader && HasNewVersion(program->geometryShaderFilename, &program->geometryShaderTime)) ||
           HasNewVersion(program->fragmentShaderFilename, &program->fragmentShaderTime);
}

internal void CheckForNewShaders(TransientDrawingInfo *info)
{
    if (HasNewVersion(&info->objectShader) || HasNewVersion(&info->instancedObjectShader) ||
        HasNewVersion(&info->colorShader) || HasNewVersion(&info->outlineShader) ||
        HasNewVersion(&info->textureShader) || HasNewVersion(&info->glassShader) ||
        HasNewVersion(&info->postProcessShader) || HasNewVersion(&info->skyboxShader) ||
        HasNewVersion(&info->geometryShader))
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

void RenderWithColorShader(TransientDrawingInfo *transientInfo, PersistentDrawingInfo *persistentInfo)
{
    u32 shaderProgram = transientInfo->colorShader.id;
    glUseProgram(shaderProgram);

    glBindVertexArray(transientInfo->cubeVao);

    for (u32 lightIndex = 0; lightIndex < NUM_POINTLIGHTS; lightIndex++)
    {
        PointLight *curLight = &persistentInfo->pointLights[lightIndex];

        glEnable(GL_STENCIL_TEST);
        glStencilMask(0xff);
        glStencilFunc(GL_ALWAYS, 1, 0xff);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

        SetShaderUniformVec3(shaderProgram, "color", curLight->diffuse);
        RenderObject(transientInfo->cubeVao, 36, curLight->position, shaderProgram);

        glStencilMask(0x00);
        glStencilFunc(GL_NOTEQUAL, 1, 0xff);

        glm::vec4 stencilColor = glm::vec4(0.f, 0.f, 1.f, 1.f);
        SetShaderUniformVec3(shaderProgram, "color", stencilColor);
        RenderObject(transientInfo->cubeVao, 36, curLight->position, shaderProgram, 0.f, 1.1f);

        glDisable(GL_STENCIL_TEST);
    }
}

void DrawScene(CameraInfo *cameraInfo, TransientDrawingInfo *transientInfo, PersistentDrawingInfo *persistentInfo,
               u32 fbo, u32 quadTexture, HWND window, Arena *listArena, Arena *tempArena, bool dynamicEnvPass = false,
               bool shadowMapPass = false);

void RenderModel(Model *model, u32 shaderProgram, u32 skyboxTexture = 0, u32 numInstances = 1)
{
    glUseProgram(shaderProgram);

    for (u32 i = 0; i < model->meshCount; i++)
    {
        glBindVertexArray(model->vaos[i]);

        Mesh *mesh = &model->meshes[i];
        for (u32 textureSlot = 0; textureSlot < mesh->numTextures; textureSlot++)
        {
            glActiveTexture(GL_TEXTURE0 + textureSlot);
            glBindTexture(GL_TEXTURE_2D, mesh->textures[textureSlot].id);
        }
        if (skyboxTexture > 0)
        {
            // Skybox contribution.
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture);
        }

        if (numInstances == 1)
        {
            // Model matrix: transforms vertices from local to world space.
            glm::mat4 modelMatrix = glm::mat4(1.f);
            modelMatrix = glm::translate(modelMatrix, glm::vec3(0.f));

            glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(modelMatrix)));

            SetShaderUniformMat4(shaderProgram, "modelMatrix", &modelMatrix);
            SetShaderUniformMat3(shaderProgram, "normalMatrix", &normalMatrix);

            glDrawElements(GL_TRIANGLES, mesh->verticesSize / sizeof(Vertex), GL_UNSIGNED_INT, 0);
        }
        else
        {
            SetShaderUniformFloat(shaderProgram, "time", Win32GetTime());
            glDrawElementsInstanced(GL_TRIANGLES, mesh->verticesSize / sizeof(Vertex), GL_UNSIGNED_INT, 0,
                                    numInstances);
        }
    }
}

void RenderShaderPass(ShaderProgram *shaderProgram)
{
    for (u32 i = 0; i < shaderProgram->numObjects; i++)
    {
        Object *curObject = shaderProgram->shaderPassObjects[i];
        RenderObject(curObject->VAO, curObject->numIndices, curObject->position, shaderProgram->id);
    }
    for (u32 i = 0; i < shaderProgram->numModels; i++)
    {
        RenderModel(shaderProgram->shaderPassModels[i], shaderProgram->id);
    }
}

void FlipImage(u8 *data, s32 width, s32 height, u32 bytesPerPixel, Arena *tempArena)
{
    u32 stride = width * bytesPerPixel;
    u8 *tmp = (u8 *)ArenaPush(tempArena, stride);
    u8 *end = data + width * height * bytesPerPixel;
    for (s32 i = 0; i < height / 2; i++)
    {
        memcpy_s(tmp, stride, data + i * stride, stride);
        memcpy_s(data + i * stride, stride, end - (1 + i) * stride, stride);
        memcpy_s(end - (1 + i) * stride, stride, tmp, stride);
    }
    ArenaPop(tempArena, stride);
}

internal void SetObjectShaderUniforms(u32 shaderProgram, CameraInfo *cameraInfo, PersistentDrawingInfo *persistentInfo)
{
    glUseProgram(shaderProgram);

    SetShaderUniformFloat(shaderProgram, "material.shininess", persistentInfo->materialShininess);
    SetShaderUniformInt(shaderProgram, "blinn", persistentInfo->blinn);

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
}

internal void RenderWithObjectShader(CameraInfo *cameraInfo, TransientDrawingInfo *transientInfo,
                                     PersistentDrawingInfo *persistentInfo, HWND window, Arena *listArena,
                                     Arena *tempArena, bool dynamicEnvPass = false)
{
    // TODO: find a proper way to parameterize dynamic environment mapping. We only want certain objects
    // to reflect the environment, not all of them. It should depend on their shininess.
    local_persist EnvironmentMap dynamicEnvMap;
#if 0
    // Dynamic environment mapping.
    {
        s32 sideLength = 728;

        if (!dynamicEnvMap.initialized)
        {
            for (u32 i = 0; i < 6; i++)
            {
                GLenum framebufferStatus =
                    CreateFramebuffer(sideLength, sideLength, &dynamicEnvMap.FBOs[i], &dynamicEnvMap.quads[i]);
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

            float orientations[6][2] = {
                {-PI / 2.f, 0.f}, {PI / 2.f, 0.f}, {0.f, PI / 2}, {0.f, -PI / 2}, {PI, 0.f}, {0.f, 0.f},
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
                FlipImage(data, sideLength, sideLength, 3, tempArena);
                glBindTexture(GL_TEXTURE_CUBE_MAP, dynamicEnvMap.skyboxTexture);
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, sideLength, sideLength, 0, GL_RGB, GL_UNSIGNED_BYTE,
                             data);
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
#endif

    u32 shaderProgram = transientInfo->objectShader.id;
    SetObjectShaderUniforms(shaderProgram, cameraInfo, persistentInfo);
    
    RenderShaderPass(&transientInfo->objectShader);
}

void RenderWithGeometryShader(TransientDrawingInfo *transientInfo)
{
    u32 shaderProgram = transientInfo->geometryShader.id;
    glUseProgram(shaderProgram);
    
    SetShaderUniformVec3(shaderProgram, "color", glm::vec3(1.f, 1.f, 0.f));

    RenderShaderPass(&transientInfo->geometryShader);
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
    RenderShaderPass(&transientInfo->textureShader);
    glDisable(GL_CULL_FACE);
}

void RenderWithGlassShader(CameraInfo *cameraInfo, TransientDrawingInfo *transientInfo,
                           PersistentDrawingInfo *persistentInfo, Arena *listArena, Arena *tempArena)
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
    // TODO: account for in refactor.
    // for (u32 i = 0; i < NUM_OBJECTS; i++)
    // {
    //     f32 dist = glm::distance(cameraInfo->pos, persistentInfo->windowPos[i]);
    //     Insert(&list, dist, persistentInfo->windowPos[i], listArena, tempArena);
    // }

    // for (u32 i = 0; i < NUM_OBJECTS; i++)
    // {
    //     glm::vec3 pos = GetValue(&list, i);

    //     RenderObject(transientInfo->mainQuadVao, 6, pos, shaderProgram, cameraInfo->yaw);
    // }
    ArenaClear(listArena);
    memset(listArena->memory, 0, listArena->size);
    ArenaClear(tempArena);

    glDisable(GL_BLEND);
}

void DrawScene(CameraInfo *cameraInfo, TransientDrawingInfo *transientInfo, PersistentDrawingInfo *persistentInfo,
               u32 fbo, u32 quadTexture, HWND window, Arena *listArena, Arena *tempArena, bool dynamicEnvPass,
               bool shadowMapPass)
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

    f32 nearPlaneDistance = shadowMapPass ? 1.f : .1f;
    f32 farPlaneDistance = shadowMapPass ? 7.5f : 150.f;

    glm::mat4 viewMatrix = shadowMapPass
                               ? glm::lookAt(glm::vec3(-2.f, 4.f, -1.f), glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f))
                               : LookAt(cameraInfo, cameraTarget, cameraUpVec, farPlaneDistance);

    glm::mat4 projectionMatrix = shadowMapPass
                                     ? glm::ortho(-10.f, 10.f, -10.f, 10.f, nearPlaneDistance, farPlaneDistance)
                                     : glm::perspective(glm::radians(cameraInfo->fov), cameraInfo->aspectRatio,
                                                        nearPlaneDistance, farPlaneDistance);

    glBindBuffer(GL_UNIFORM_BUFFER, transientInfo->matricesUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, 64, &viewMatrix);
    glBufferSubData(GL_UNIFORM_BUFFER, 64, 64, &projectionMatrix);

    // TODO: fix effect of outlining on meshes that appear between the camera and the outlined object.

    // Point lights.
    RenderWithColorShader(transientInfo, persistentInfo);

    // Objects.
    RenderWithObjectShader(cameraInfo, transientInfo, persistentInfo, window, listArena, tempArena, dynamicEnvPass);
    RenderWithGeometryShader(transientInfo);

    // Textured cubes.
    RenderWithTextureShader(cameraInfo, transientInfo, persistentInfo);

    // Windows.
    // RenderWithGlassShader(cameraInfo, transientInfo, persistentInfo, listArena, tempArena);

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

void DrawDebugWindow(CameraInfo *cameraInfo, TransientDrawingInfo *transientInfo, PersistentDrawingInfo *persistentInfo)
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

    ImGui::Separator();

    ImGui::SliderFloat("Gamma correction", &persistentInfo->gamma, 0.f, 5.f);

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Shading"))
    {
        ImGui::InputFloat("Material shininess", &persistentInfo->materialShininess);
        ImGui::Text("Using %s shading", persistentInfo->blinn ? "Blinn-Phong" : "Phong");
        if (ImGui::Button("Toggle"))
        {
            persistentInfo->blinn = !persistentInfo->blinn;
        }
    }

    ImGui::Separator();

    ImGui::Text("Multisampling: %s", glIsEnabled(GL_MULTISAMPLE) ? "enabled" : "disabled");
    ImGui::SameLine();
    if (ImGui::Button("Toggle multisampling"))
    {
        if (glIsEnabled(GL_MULTISAMPLE))
        {
            glDisable(GL_MULTISAMPLE);
        }
        else
        {
            glEnable(GL_MULTISAMPLE);
        }
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

    // TODO: account for in refactor.
    // if (ImGui::CollapsingHeader("Windows"))
    // {
    //     glm::vec3 *windows = persistentInfo->windowPos;
    //     for (u32 index = 0; index < NUM_OBJECTS; index++)
    //     {
    //         ImGui::PushID(id++);
    //         char treeName[32];
    //         sprintf_s(treeName, "Window #%i", index);
    //         if (ImGui::TreeNode(treeName))
    //         {
    //             ImGui::SliderFloat3("Position", glm::value_ptr(windows[index]), -10.f, 10.f);
    //             ImGui::TreePop();
    //         }
    //         ImGui::PopID();
    //     }
    // }

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
    DrawScene(cameraInfo, transientInfo, persistentInfo, transientInfo->mainFBO, transientInfo->mainQuad, window,
              listArena, tempArena);

    CameraInfo rearViewCamera = *cameraInfo;
    rearViewCamera.yaw += PI;
    rearViewCamera.pitch *= -1.f;
    rearViewCamera.forwardVector = GetCameraForwardVector(&rearViewCamera);
    rearViewCamera.rightVector = GetCameraRightVector(&rearViewCamera);

    // Rear-view pass.
    DrawScene(&rearViewCamera, transientInfo, persistentInfo, transientInfo->rearViewFBO, transientInfo->rearViewQuad,
              window, listArena, tempArena);

    RECT clientRect;
    GetClientRect(window, &clientRect);
    s32 width = clientRect.right;
    s32 height = clientRect.bottom;

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, transientInfo->postProcessingFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, transientInfo->mainFBO);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    // NOTE: in the case of the rear-view mirror, since a multisampled framebuffer cannot be blitted to
    // only a portion of a non-multisampled framebuffer, there are 3 options:
    // 1. blit a non-multisampled framebuffer to a portion of the main buffer;
    // 2. draw the texture attached to a non-multisampled buffer to the screen;
    // 3. draw the texture attached to a multisampled buffer to the screen.
    // I went with option 2 here since it results in less aliasing than option 1 and unlike option 3
    // doesn't require maintaining a second shader to apply the same post-processing effects to the
    // multisampled rear-view framebuffer (see: postprocess_ms.fs).
    // See also: ResizeGLViewport().
    /*
    glBindFramebuffer(GL_READ_FRAMEBUFFER, transientInfo->rearViewFBO);
    glBlitFramebuffer(0, 0, width, height, width / 4, height * 17 / 20, width * 3 / 4, height, GL_COLOR_BUFFER_BIT,
    GL_NEAREST);
    */
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(1.f, 1.f, 1.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Main quad.
    {
        u32 shaderProgram = transientInfo->postProcessShader.id;
        glUseProgram(shaderProgram);

        SetShaderUniformFloat(shaderProgram, "gamma", persistentInfo->gamma);

        glDisable(GL_DEPTH_TEST);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, transientInfo->postProcessingQuad);

        glm::mat4 identity = glm::mat4(1.f);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, 64, &identity);
        glBufferSubData(GL_UNIFORM_BUFFER, 64, 64, &identity);

        glBindVertexArray(transientInfo->postProcessingQuadVao);

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

        SetShaderUniformFloat(shaderProgram, "gamma", persistentInfo->gamma);

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

    DrawDebugWindow(cameraInfo, transientInfo, persistentInfo);

    if (!SwapBuffers(hdc))
    {
        if (MessageBoxW(window, L"Failed to swap buffers", L"OpenGL error", MB_OK) == S_OK)
        {
            *running = false;
        }
    }
}

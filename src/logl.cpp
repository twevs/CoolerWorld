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
    char infoLog[1024];
    glGetShaderiv(*shaderID, GL_COMPILE_STATUS, &success);
    glGetShaderInfoLog(*shaderID, 1024, NULL, infoLog);
    DebugPrintA("Shader compilation infolog for %s: %s\n", shaderFilename, infoLog);
    if (!success || strstr(infoLog, "Warning"))
    {
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
    char infoLog[1024];
    glGetProgramiv(*programID, GL_LINK_STATUS, &success);
    glGetProgramInfoLog(*programID, 1024, NULL, infoLog);
    DebugPrintA("Shader program creation infolog: %s\n", infoLog);
    if (!success || strstr(infoLog, "Warning"))
    {
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

internal void SetShaderUniformInt(u32 shaderProgram, const char *uniformName, s32 value)
{
    glUniform1i(glGetUniformLocation(shaderProgram, uniformName), value);
}

internal void SetShaderUniformFloat(u32 shaderProgram, const char *uniformName, float value)
{
    glUniform1f(glGetUniformLocation(shaderProgram, uniformName), value);
}

internal void SetShaderUniformVec2(u32 shaderProgram, const char *uniformName, glm::vec2 vector)
{
    glUniform2fv(glGetUniformLocation(shaderProgram, uniformName), 1, glm::value_ptr(vector));
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

internal bool CreateShaderProgram(ShaderProgram *program, const char *vertexShaderFilename,
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
    if (glIsProgram(program->id))
    {
        memcpy(newShaderProgram.objectIndices, program->objectIndices, program->numObjects * sizeof(u32));
        newShaderProgram.numObjects = program->numObjects;
        memcpy(newShaderProgram.modelIndices, program->modelIndices, program->numModels * sizeof(u32));
        newShaderProgram.numModels = program->numModels;
        glDeleteProgram(program->id);
    }

    *program = newShaderProgram;
    return true;
}

internal bool CreateShaderPrograms(TransientDrawingInfo *info)
{
    if (!CreateShaderProgram(&info->gBufferShader, "gbuffer.vs", "gbuffer.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->ssaoShader, "vertex_shader.vs", "ssao.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->ssaoBlurShader, "vertex_shader.vs", "ssao_blur.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->nonPointLightingShader, "vertex_shader.vs", "fragment_shader.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->pointLightingShader, "vertex_shader.vs", "point_lighting.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->dirDepthMapShader, "depth_map.vs", "depth_map.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->spotDepthMapShader, "spot_depth_map.vs", "depth_map.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->pointDepthMapShader, "depth_cube_map.vs", "depth_cube_map.fs", "depth_cube_map.gs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->instancedObjectShader, "instanced.vs", "fragment_shader.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->colorShader, "vertex_shader.vs", "color.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->outlineShader, "vertex_shader.vs", "outline.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->glassShader, "vertex_shader.vs", "glass.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->textureShader, "vertex_shader.vs", "texture.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->postProcessShader, "vertex_shader.vs", "postprocess.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->skyboxShader, "cubemap.vs", "cubemap.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->geometryShader, "vertex_shader_geometry.vs", "color.fs", "vis_normals.gs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->gaussianShader, "vertex_shader.vs", "gaussian.fs"))
    {
        return false;
    }

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
    char label[64];
    sprintf_s(label, "Texture: %s", filename);
    glObjectLabel(GL_TEXTURE, texture, -1, label);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    bool alpha = (numChannels == 4);
    GLenum pixelFormat = alpha ? GL_RGBA : (numChannels == 3) ? GL_RGB : GL_RED;
    if (sRGB)
    {
        GLenum internalFormat = alpha ? GL_SRGB_ALPHA : GL_SRGB;
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, pixelFormat, GL_UNSIGNED_BYTE, textureData);
    }
    else
    {
        glPixelStorei(GL_UNPACK_ALIGNMENT, alpha ? 4 : 1);
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

internal TextureType GetTextureTypeFromAssimp(aiTextureType type)
{
    if (type == aiTextureType_DIFFUSE)
    {
        return TextureType::Diffuse;
    }
    if (type == aiTextureType_SPECULAR)
    {
        return TextureType::Specular;
    }
    if (type == aiTextureType_HEIGHT)
    {
        return TextureType::Normals;
    }
    if (type == aiTextureType_DISPLACEMENT)
    {
        return TextureType::Displacement;
    }
    myAssert(false);
    return TextureType::Diffuse;
}

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
            texture->type = GetTextureTypeFromAssimp(type);
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

        result.vertices[i].tangent.x = mesh->mTangents[i].x;
        result.vertices[i].tangent.y = mesh->mTangents[i].y;
        result.vertices[i].tangent.z = mesh->mTangents[i].z;

        result.vertices[i].bitangent.x = mesh->mBitangents[i].x;
        result.vertices[i].bitangent.y = mesh->mBitangents[i].y;
        result.vertices[i].bitangent.z = mesh->mBitangents[i].z;

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
        u32 numNormals = material->GetTextureCount(aiTextureType_HEIGHT);
        u32 numDisp = material->GetTextureCount(aiTextureType_DISPLACEMENT);
        u32 numTextures = numDiffuse + numSpecular + numNormals + numDisp;

        u64 texturesSize = sizeof(Texture) * numTextures;
        result.textures = (Texture *)ArenaPush(meshDataArena, texturesSize);

        LoadTextures(&result, numDiffuse, material, aiTextureType_DIFFUSE, texturesArena, loadedTextures);
        LoadTextures(&result, numSpecular, material, aiTextureType_SPECULAR, texturesArena, loadedTextures);
        LoadTextures(&result, numNormals, material, aiTextureType_HEIGHT, texturesArena, loadedTextures);
        if (numDisp > 0)
        {
            LoadTextures(&result, numDisp, material, aiTextureType_DISPLACEMENT, texturesArena, loadedTextures);
        }
    }

    return result;
}

internal void ProcessNode(aiNode *node, const aiScene *scene, Mesh *meshes, u32 *meshCount, Arena *texturesArena,
                          LoadedTextures *loadedTextures, Arena *meshDataArena)
{
    for (u32 i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
        Mesh processedMesh = ProcessMesh(mesh, scene, texturesArena, loadedTextures, meshDataArena);
        aiMatrix4x4 trans = node->mTransformation;
        glm::mat4 rowMajorTrans = {trans.a1, trans.a2, trans.a3, trans.a4, trans.b1, trans.b2, trans.b3, trans.b4,
                                   trans.c1, trans.c2, trans.c3, trans.c4, trans.d1, trans.d2, trans.d3, trans.d4};
        processedMesh.relativeTransform = glm::transpose(rowMajorTrans);
        meshes[*meshCount] = processedMesh;
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

    u32 vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

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
                         Arena *meshDataArena, f32 scale = 1.f)
{
    Model result = {};

    Assimp::Importer importer;
    const aiScene *scene =
        importer.ReadFile(filename, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
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
    result.scale = glm::vec3(scale);

    return result;
}

internal void LoadShaderPass(FILE *file, ShaderProgram *shader)
{
    fread(&shader->numObjects, sizeof(u32), 1, file);
    fread(shader->objectIndices, sizeof(u32), shader->numObjects, file);
    fread(&shader->numModels, sizeof(u32), 1, file);
    fread(shader->modelIndices, sizeof(u32), shader->numModels, file);
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
    LoadShaderPass(file, &transientInfo->gBufferShader);
    LoadShaderPass(file, &transientInfo->ssaoShader);
    LoadShaderPass(file, &transientInfo->ssaoBlurShader);
    LoadShaderPass(file, &transientInfo->dirDepthMapShader);
    LoadShaderPass(file, &transientInfo->spotDepthMapShader);
    LoadShaderPass(file, &transientInfo->pointDepthMapShader);
    LoadShaderPass(file, &transientInfo->instancedObjectShader);
    LoadShaderPass(file, &transientInfo->colorShader);
    LoadShaderPass(file, &transientInfo->outlineShader);
    LoadShaderPass(file, &transientInfo->glassShader);
    LoadShaderPass(file, &transientInfo->textureShader);
    LoadShaderPass(file, &transientInfo->postProcessShader);
    LoadShaderPass(file, &transientInfo->skyboxShader);
    LoadShaderPass(file, &transientInfo->geometryShader);
    fread(cameraInfo, sizeof(CameraInfo), 1, file);

    fclose(file);

    return true;
}

#define MAX_ATTACHMENTS 4

struct FramebufferOptions
{
    GLenum internalFormat = GL_RGB;
    GLenum type = GL_UNSIGNED_BYTE;
    GLenum filteringMethod = GL_LINEAR;
    GLenum wrapMode = GL_REPEAT;
    f32 borderColor[4] = {1.f, 1.f, 1.f, 1.f};
};

internal GLenum CreateFramebuffer(const char *label, s32 width, s32 height, u32 *fbo, u32 *quadTextures,
                                  u32 numTextures, u32 *rbo, FramebufferOptions *options)
{
    myAssert(numTextures <= MAX_ATTACHMENTS);

    glGenFramebuffers(1, fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
    char framebufferLabel[64];
    sprintf_s(framebufferLabel, "Framebuffer: %s", label);
    glObjectLabel(GL_FRAMEBUFFER, *fbo, -1, framebufferLabel);

    glGenTextures(numTextures, quadTextures);
    GLenum attachments[MAX_ATTACHMENTS] = {};
    for (u32 i = 0; i < numTextures; i++)
    {
        glBindTexture(GL_TEXTURE_2D, quadTextures[i]);
        char textureLabel[64];
        sprintf_s(textureLabel, "Framebuffer: %s - Attached texture %i", label, i);
        glObjectLabel(GL_TEXTURE, quadTextures[i], -1, textureLabel);

        GLenum internalFormat = options[i].internalFormat;
        bool depthMap = (internalFormat == GL_DEPTH_COMPONENT);
        bool hdr = (internalFormat == GL_RGBA16F);
        myAssert(!(depthMap && hdr));
        myAssert(!(depthMap && (numTextures > 1)));

        GLenum format = hdr ? GL_RGBA : internalFormat;
        myAssert(!((depthMap || hdr) && (options->type != GL_FLOAT)));
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, options->type, NULL);
        GLint filteringMethod = options[i].filteringMethod;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filteringMethod);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filteringMethod);
        GLenum wrapMode = options[i].wrapMode;
        if (wrapMode != GL_REPEAT)
        {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode);

            if (wrapMode == GL_CLAMP_TO_BORDER)
            {
                glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, options[i].borderColor);
            }
        }
        // TODO: non-depth map texture wrapping parameters?
        glBindTexture(GL_TEXTURE_2D, 0);

        GLenum attachment = depthMap ? GL_DEPTH_ATTACHMENT : (GL_COLOR_ATTACHMENT0 + i);
        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, quadTextures[i], 0);

        attachments[i] = attachment;
    }

    if (options[0].internalFormat == GL_DEPTH_COMPONENT)
    {
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }
    else
    {
        glDrawBuffers(numTextures, attachments);
        glGenRenderbuffers(1, rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, *rbo);
        char renderbufferLabel[64];
        sprintf_s(renderbufferLabel, "Renderbuffer: %s", label);
        glObjectLabel(GL_RENDERBUFFER, *rbo, -1, renderbufferLabel);
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

internal u32 CreateVAO(f32 *vertices, u32 verticesSize, s32 *elemCounts, u32 elemCountsSize, u32 *indices,
                       u32 indicesSize)
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

internal void SetUpAsteroids(Model *asteroid)
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

    // NOTE: I originally used AppendToVAO() as I did not know that a VAO could have attribute
    // pointers into several VBOs. Out of curiosity, I profiled both methods and there is no
    // performance difference.
    // TODO: account for tangents and bitangents.
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

internal GLenum CreateDepthCubemap(const char *label, u32 *depthCubemap, u32 *depthCubemapFBO)
{
    glGenTextures(1, depthCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, *depthCubemap);
    char textureLabel[64];
    sprintf_s(textureLabel, "Texture (cube map): %s", label);
    glObjectLabel(GL_TEXTURE, *depthCubemap, -1, textureLabel);
    for (u32 i = 0; i < 6; i++)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT, POINT_SHADOW_MAP_SIZE,
                     POINT_SHADOW_MAP_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, depthCubemapFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, *depthCubemapFBO);
    char framebufferLabel[64];
    sprintf_s(framebufferLabel, "Framebuffer (cube map): %s", label);
    glObjectLabel(GL_FRAMEBUFFER, *depthCubemapFBO, -1, framebufferLabel);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, *depthCubemap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    return glCheckFramebufferStatus(GL_FRAMEBUFFER);
}

internal void CreateFramebuffers(HWND window, TransientDrawingInfo *transientInfo)
{
    RECT clientRect;
    GetClientRect(window, &clientRect);
    s32 width = clientRect.right;
    s32 height = clientRect.bottom;

    // TODO: don't create RBOs where they aren't needed.

    FramebufferOptions hdrBuffer = {};
    hdrBuffer.internalFormat = GL_RGBA16F;
    hdrBuffer.type = GL_FLOAT;

    // Main G-buffer, with 3 colour buffers:
    // 0 = position buffer, 1 = normal buffer, 2 = albedo buffer.
    FramebufferOptions gBufferOptions[3] = {hdrBuffer, hdrBuffer, hdrBuffer};
    auto *positionBuffer = &gBufferOptions[0];
    positionBuffer->filteringMethod = GL_NEAREST;
    positionBuffer->wrapMode = GL_CLAMP_TO_EDGE;

    GLenum mainFramebufferStatus =
        CreateFramebuffer("Main G-buffer", width, height, &transientInfo->mainFBO, transientInfo->mainQuads, 3,
                          &transientInfo->mainRBO, gBufferOptions);
    myAssert(mainFramebufferStatus == GL_FRAMEBUFFER_COMPLETE);

    GLenum rearViewFramebufferStatus =
        CreateFramebuffer("Rear-view G-buffer", width, height, &transientInfo->rearViewFBO,
                          transientInfo->rearViewQuads, 3, &transientInfo->rearViewRBO, gBufferOptions);
    myAssert(rearViewFramebufferStatus == GL_FRAMEBUFFER_COMPLETE);

    FramebufferOptions ssaoFramebufferOptions = {};
    ssaoFramebufferOptions.internalFormat = GL_RED;
    ssaoFramebufferOptions.type = GL_FLOAT;
    ssaoFramebufferOptions.filteringMethod = GL_NEAREST;

    GLenum ssaoFramebufferStatus =
        CreateFramebuffer("SSAO", width, height, &transientInfo->ssaoFBO, &transientInfo->ssaoQuad, 1,
                          &transientInfo->ssaoRBO, &ssaoFramebufferOptions);
    myAssert(ssaoFramebufferStatus == GL_FRAMEBUFFER_COMPLETE);
    GLenum ssaoBlurFramebufferStatus =
        CreateFramebuffer("SSAO blur", width, height, &transientInfo->ssaoBlurFBO, &transientInfo->ssaoBlurQuad, 1,
                          &transientInfo->ssaoBlurRBO, &ssaoFramebufferOptions);
    myAssert(ssaoFramebufferStatus == GL_FRAMEBUFFER_COMPLETE);

    // 0 = HDR colour buffer, 1 = bloom threshold buffer.
    FramebufferOptions lightingFramebufferOptions[2] = {hdrBuffer, hdrBuffer};

    GLenum lightingFramebufferStatus =
        CreateFramebuffer("Main lighting pass", width, height, &transientInfo->lightingFBO,
                          transientInfo->lightingQuads, 2, &transientInfo->lightingRBO, lightingFramebufferOptions);
    myAssert(lightingFramebufferStatus == GL_FRAMEBUFFER_COMPLETE);

    GLenum rearViewLightingFramebufferStatus = CreateFramebuffer(
        "Rear-view lighting pass", width, height, &transientInfo->rearViewLightingFBO,
        transientInfo->rearViewLightingQuads, 2, &transientInfo->rearViewLightingRBO, lightingFramebufferOptions);
    myAssert(rearViewLightingFramebufferStatus == GL_FRAMEBUFFER_COMPLETE);

    GLenum postProcessingFramebufferStatus =
        CreateFramebuffer("Post-processing", width, height, &transientInfo->postProcessingFBO,
                          &transientInfo->postProcessingQuad, 1, &transientInfo->postProcessingRBO, &hdrBuffer);
    myAssert(postProcessingFramebufferStatus == GL_FRAMEBUFFER_COMPLETE);

    FramebufferOptions depthBufferOptions = {};
    depthBufferOptions.internalFormat = GL_DEPTH_COMPONENT;
    depthBufferOptions.type = GL_FLOAT;
    depthBufferOptions.filteringMethod = GL_NEAREST;
    depthBufferOptions.wrapMode = GL_CLAMP_TO_BORDER;

    GLenum dirDepthMapFramebufferStatus = CreateFramebuffer(
        "Directional light shadow map", DIR_SHADOW_MAP_SIZE, DIR_SHADOW_MAP_SIZE, &transientInfo->dirShadowMapFBO,
        &transientInfo->dirShadowMapQuad, 1, &transientInfo->dirShadowMapRBO, &depthBufferOptions);
    myAssert(dirDepthMapFramebufferStatus == GL_FRAMEBUFFER_COMPLETE);

    GLenum spotDepthMapFramebufferStatus =
        CreateFramebuffer("Spot light shadow map", width, height, &transientInfo->spotShadowMapFBO,
                          &transientInfo->spotShadowMapQuad, 1, &transientInfo->spotShadowMapRBO, &depthBufferOptions);
    myAssert(spotDepthMapFramebufferStatus == GL_FRAMEBUFFER_COMPLETE);

    for (u32 i = 0; i < NUM_POINTLIGHTS; i++)
    {
        char label[32];
        sprintf_s(label, "Point light depth map %i", i);
        GLenum pointDepthCubemapStatus =
            CreateDepthCubemap(label, &transientInfo->pointShadowMapQuad[i], &transientInfo->pointShadowMapFBO[i]);
        myAssert(pointDepthCubemapStatus == GL_FRAMEBUFFER_COMPLETE);
    }

    for (u32 i = 0; i < 2; i++)
    {
        char label[32];
        sprintf_s(label, "Gaussian ping-pong buffer %i", i);
        GLenum gaussianFramebufferStatus =
            CreateFramebuffer(label, width, height, &transientInfo->gaussianFBOs[i], &transientInfo->gaussianQuads[i],
                              1, &transientInfo->gaussianRBOs[i], &hdrBuffer);
        myAssert(gaussianFramebufferStatus == GL_FRAMEBUFFER_COMPLETE);
    }
}

internal void CreateSkybox(TransientDrawingInfo *transientInfo)
{
    u32 skyboxTexture;
    glGenTextures(1, &skyboxTexture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture);
    char label[] = "Texture (cube map): skybox";
    glObjectLabel(GL_TEXTURE, skyboxTexture, -1, label);

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

internal u32 AddModel(const char *filename, TransientDrawingInfo *transientInfo, s32 *elemCounts, u32 elemCountsSize,
                      Arena *texturesArena, Arena *meshDataArena, f32 scale = 1.f)
{
    u32 modelIndex = transientInfo->numModels;
    transientInfo->models[modelIndex] =
        LoadModel(filename, elemCounts, elemCountsSize, texturesArena, meshDataArena, scale);
    transientInfo->numModels++;
    myAssert(transientInfo->numModels <= MAX_MODELS);
    return modelIndex;
}

internal Texture CreateTexture(const char *filename, TextureType type, GLenum wrapMode = GL_REPEAT)
{
    Texture result = {};
    if (strlen(filename) > 0)
    {
        result.id = CreateTextureFromImage(filename, type == TextureType::Diffuse, wrapMode);
        result.type = type;
        result.hash = fnv1a((u8 *)filename, strlen(filename));
    }
    return result;
}

internal Textures CreateTextures(const char *diffusePath, const char *specularPath = "", const char *normalsPath = "",
                                 const char *displacementPath = "")
{
    Textures result = {};
    result.diffuse = CreateTexture(diffusePath, TextureType::Diffuse);
    result.specular = CreateTexture(specularPath, TextureType::Specular);
    result.normals = CreateTexture(normalsPath, TextureType::Normals);
    result.displacement = CreateTexture(displacementPath, TextureType::Displacement);
    return result;
}

internal u32 AddObject(TransientDrawingInfo *transientInfo, u32 vao, u32 numIndices, glm::vec3 position,
                       Textures *textures = nullptr)
{
    u32 objectIndex = transientInfo->numObjects;
    transientInfo->objects[objectIndex] = {vao, numIndices, position};
    if (textures)
    {
        transientInfo->objects[objectIndex].textures = *textures;
    }
    transientInfo->numObjects++;
    myAssert(transientInfo->numObjects <= MAX_OBJECTS);
    return objectIndex;
}

internal void AddModelToShaderPass(ShaderProgram *shader, u32 modelIndex)
{
    shader->modelIndices[shader->numModels] = modelIndex;
    shader->numModels++;
    myAssert(shader->numModels <= MAX_MODELS);
}

internal void AddObjectToShaderPass(ShaderProgram *shader, u32 objectIndex)
{
    shader->objectIndices[shader->numObjects] = objectIndex;
    shader->numObjects++;
    myAssert(shader->numObjects <= MAX_OBJECTS);
}

// Following Lengyel's "Foundations of Game Engine Development", vol. 1 (2016), p. 35.
internal glm::vec3 Reject(glm::vec3 a, glm::vec3 b)
{
    return (a - b * dot(a, b) / dot(b, b));
}

// Following Lengyel's "Foundations of Game Engine Development", vol. 2 (2019), pp. 114-5.
internal void CalculateTangents(Vertex *vertices, u32 numVertices, u32 *indices, u32 numIndices, Arena *arena)
{
    u64 allocatedSize = sizeof(glm::vec3) * numVertices * 2;
    glm::vec3 *tangents = (glm::vec3 *)ArenaPush(arena, allocatedSize);
    glm::vec3 *bitangents = tangents + numVertices;

    for (u32 i = 0; i < numIndices; i += 3)
    {
        u32 i0 = indices[i];
        u32 i1 = indices[i + 1];
        u32 i2 = indices[i + 2];

        glm::vec3 p0 = vertices[i0].position;
        glm::vec3 p1 = vertices[i1].position;
        glm::vec3 p2 = vertices[i2].position;

        glm::vec2 w0 = vertices[i0].texCoords;
        glm::vec2 w1 = vertices[i1].texCoords;
        glm::vec2 w2 = vertices[i2].texCoords;

        glm::vec3 e1 = p1 - p0;
        glm::vec3 e2 = p2 - p0;
        f32 x1 = w1.x - w0.x;
        f32 x2 = w2.x - w0.x;
        f32 y1 = w1.y - w0.y;
        f32 y2 = w2.y - w0.y;

        f32 r = 1.f / (x1 * y2 - x2 * y1);
        glm::vec3 tangent = (e1 * y2 - e2 * y1) * r;
        glm::vec3 bitangent = (e2 * x1 - e1 * x2) * r;

        tangents[i0] += tangent;
        tangents[i1] += tangent;
        tangents[i2] += tangent;

        bitangents[i0] += bitangent;
        bitangents[i1] += bitangent;
        bitangents[i2] += bitangent;
    }

    // Orthogonalize each normal and calculate its handedness.
    for (u32 i = 0; i < numVertices; i++)
    {
        glm::vec4 result;

        glm::vec3 tangent = tangents[i];
        glm::vec3 bitangent = bitangents[i];
        glm::vec3 normal = vertices[i].normal;

        // Gram-Schmidt orthogonalization.
        glm::vec3 tan = normalize(Reject(tangent, normal));
        result.x = tan.x;
        result.y = tan.y;
        result.z = tan.z;
        result.w = (dot(cross(tangent, bitangent), normal) > 0.f) ? 1.f : -1.f;

        // TODO: use a vec4 type for the tangent so we can forego storing the bitangent and can access
        // the handedness in the shader?
        vertices[i].tangent = tan;
        vertices[i].bitangent = normalize(bitangent);
    }

    ArenaPop(arena, allocatedSize);
}

internal f32 lerp(f32 a, f32 b, f32 alpha)
{
    return a + alpha * (b - a);
}

#define SSAO_KERNEL_SIZE 64
#define SSAO_NOISE_SIZE 16

internal void GenerateSSAOSamplesAndNoise(TransientDrawingInfo *transientInfo)
{
    srand((u32)Win32GetWallClock());

    glm::vec3 ssaoKernel[SSAO_KERNEL_SIZE] = {};
    for (u32 i = 0; i < SSAO_KERNEL_SIZE; i++)
    {
        f32 xOffset = ((f32)rand() / RAND_MAX) * 2.f - 1.f;
        f32 yOffset = ((f32)rand() / RAND_MAX) * 2.f - 1.f;
        f32 zOffset = ((f32)rand() / RAND_MAX);
        glm::vec3 sample(xOffset, yOffset, zOffset);
        sample = glm::normalize(sample);

        sample *= ((f32)rand() / RAND_MAX);
        f32 scale = (f32)i / SSAO_KERNEL_SIZE;
        scale = lerp(.1f, 1.f, scale * scale);
        sample *= scale;

        ssaoKernel[i] = sample;
    }

    u32 ssaoShader = transientInfo->ssaoShader.id;
    glUseProgram(ssaoShader);
    glUniform3fv(glGetUniformLocation(ssaoShader, "samples"), SSAO_KERNEL_SIZE, (f32 *)ssaoKernel);

    glm::vec3 ssaoNoise[SSAO_NOISE_SIZE] = {};
    for (u32 i = 0; i < SSAO_NOISE_SIZE; i++)
    {
        f32 x = ((f32)rand() / RAND_MAX) * 2.f - 1.f;
        f32 y = ((f32)rand() / RAND_MAX) * 2.f - 1.f;
        glm::vec3 sample(x, y, 0.f);
        sample = glm::normalize(sample);

        ssaoNoise[i] = sample;
    }

    u32 *ssaoNoiseTexture = &transientInfo->ssaoNoiseTexture;
    glGenTextures(1, ssaoNoiseTexture);
    glBindTexture(GL_TEXTURE_2D, *ssaoNoiseTexture);
    u32 sideLength = (u32)(sqrtf(SSAO_NOISE_SIZE));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, sideLength, sideLength, 0, GL_RGB, GL_FLOAT, &ssaoNoise);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
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

    s32 elemCounts[] = {3, 3, 2};                // Position, normal, texcoords.
    s32 bitangentElemCounts[] = {3, 3, 2, 3, 3}; // Position, normal, texcoords, tangent, bitangent.

    u32 backpackIndex = AddModel("backpack.obj", transientInfo, bitangentElemCounts, myArraySize(bitangentElemCounts),
                                 texturesArena, meshDataArena);
    u32 planetIndex = AddModel("planet.obj", transientInfo, bitangentElemCounts, myArraySize(bitangentElemCounts),
                               texturesArena, meshDataArena);
    u32 rockIndex = AddModel("rock.obj", transientInfo, bitangentElemCounts, myArraySize(bitangentElemCounts),
                             texturesArena, meshDataArena);
    u32 deccerCubesIndex = AddModel("SM_Deccer_Cubes_Textured_Complex.fbx", transientInfo, bitangentElemCounts,
                                    myArraySize(bitangentElemCounts), texturesArena, meshDataArena, .01f);
    u32 sphereIndex = AddModel("sphere.fbx", transientInfo, bitangentElemCounts, myArraySize(bitangentElemCounts),
                               texturesArena, meshDataArena);
    transientInfo->sphereModel = &transientInfo->models[sphereIndex];

    AddModelToShaderPass(&transientInfo->dirDepthMapShader, backpackIndex);
    AddModelToShaderPass(&transientInfo->spotDepthMapShader, backpackIndex);
    AddModelToShaderPass(&transientInfo->pointDepthMapShader, backpackIndex);
    AddModelToShaderPass(&transientInfo->gBufferShader, backpackIndex);
    AddModelToShaderPass(&transientInfo->geometryShader, backpackIndex);
    AddModelToShaderPass(&transientInfo->ssaoShader, backpackIndex);
    AddModelToShaderPass(&transientInfo->ssaoBlurShader, backpackIndex);

    FILE *rectFile;
    fopen_s(&rectFile, "rect.bin", "rb");
    // 4 vertices per face * 6 faces * (vec3 pos + vec3 normal + vec2 texCoords + vec3 tangent + vec3 bitangent);
    f32 rectVertices[4 * 6 * (3 + 3 + 2 + 3 + 3)] = {};
    u32 rectIndices[36];
    fread(rectVertices, sizeof(f32), myArraySize(rectVertices), rectFile);
    fread(rectIndices, sizeof(u32), myArraySize(rectIndices), rectFile);
    fclose(rectFile);
    CalculateTangents((Vertex *)rectVertices, 24, rectIndices, 36, texturesArena);

    transientInfo->cubeVao = CreateVAO(rectVertices, sizeof(rectVertices), bitangentElemCounts,
                                       myArraySize(bitangentElemCounts), rectIndices, sizeof(rectIndices));

    Vertex quadVertices[4] = {{
                                  // Top-right.
                                  glm::vec3(1.f, 1.f, 0.f), // Position.
                                  glm::vec3(0.f, 0.f, 1.f), // Normal.
                                  glm::vec2(1.f, 1.f),      // Texcoord.
                                  glm::vec3(0.f, 0.f, 0.f), // Tangent.
                                  glm::vec3(0.f, 0.f, 0.f), // Bitangent.
                              },
                              {
                                  // Bottom-right.
                                  glm::vec3(1.f, -1.f, 0.f), // Position.
                                  glm::vec3(0.f, 0.f, 1.f),  // Normal.
                                  glm::vec2(1.f, 0.f),       // Texcoord.
                                  glm::vec3(0.f, 0.f, 0.f),  // Tangent.
                                  glm::vec3(0.f, 0.f, 0.f),  // Bitangent.
                              },
                              {
                                  // Bottom-left.
                                  glm::vec3(-1.f, -1.f, 0.f), // Position.
                                  glm::vec3(0.f, 0.f, 1.f),   // Normal.
                                  glm::vec2(0.f, 0.f),        // Texcoord.
                                  glm::vec3(0.f, 0.f, 0.f),   // Tangent.
                                  glm::vec3(0.f, 0.f, 0.f),   // Bitangent.
                              },
                              {
                                  // Top-left
                                  glm::vec3(-1.f, 1.f, 0.f), // Position.
                                  glm::vec3(0.f, 0.f, 1.f),  // Normal.
                                  glm::vec2(0.f, 1.f),       // Texcoord.
                                  glm::vec3(0.f, 0.f, 0.f),  // Tangent.
                                  glm::vec3(0.f, 0.f, 0.f),  // Bitangent.
                              }};
    u32 quadIndices[] = {0, 1, 3, 1, 2, 3};
    CalculateTangents(quadVertices, 4, quadIndices, 6, texturesArena);
    u32 mainQuadVao = CreateVAO((f32 *)quadVertices, sizeof(quadVertices), bitangentElemCounts,
                                myArraySize(bitangentElemCounts), quadIndices, sizeof(quadIndices));
    transientInfo->mainQuadVao = mainQuadVao;

    f32 rearViewQuadVertices[] = {.5f,  1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, .5f,  .7f, 0.f, 0.f, 0.f, 1.f, 1.f, 0.f,
                                  -.5f, .7f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, -.5f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 1.f};
    u32 rearViewQuadIndices[] = {0, 1, 3, 1, 2, 3};
    transientInfo->rearViewQuadVao =
        CreateVAO(rearViewQuadVertices, sizeof(rearViewQuadVertices), elemCounts, myArraySize(elemCounts),
                  rearViewQuadIndices, sizeof(rearViewQuadIndices));

    srand((u32)Win32GetWallClock());

    SetUpAsteroids(&transientInfo->models[2]);

    // First we create the objects.
    PointLight *pointLights = drawingInfo->pointLights;

    // TODO: account for in refactor.
    for (u32 lightIndex = 0; lightIndex < NUM_POINTLIGHTS; lightIndex++)
    {
        pointLights[lightIndex].position = CreateRandomVec3();

        u32 attIndex = rand() % myArraySize(globalAttenuationTable);
        // NOTE: constant-range point lights makes for easier visualization of light effects.
        pointLights[lightIndex].attIndex = 4; // clamp(attIndex, 2, 6)
    }

    Textures cubeTextures = {};
    cubeTextures.diffuse = CreateTexture("window.png", TextureType::Diffuse, GL_CLAMP_TO_EDGE);
    cubeTextures.normals = CreateTexture("flat_surface_normals.png", TextureType::Normals);
    for (u32 texCubeIndex = 0; texCubeIndex < NUM_OBJECTS; texCubeIndex++)
    {
        u32 curTexCubeIndex = AddObject(transientInfo, transientInfo->cubeVao, 36, CreateRandomVec3(), &cubeTextures);
        AddObjectToShaderPass(&transientInfo->dirDepthMapShader, curTexCubeIndex);
        AddObjectToShaderPass(&transientInfo->spotDepthMapShader, curTexCubeIndex);
        AddObjectToShaderPass(&transientInfo->pointDepthMapShader, curTexCubeIndex);
        AddObjectToShaderPass(&transientInfo->gBufferShader, curTexCubeIndex);
        AddObjectToShaderPass(&transientInfo->ssaoShader, curTexCubeIndex);
        AddObjectToShaderPass(&transientInfo->ssaoBlurShader, curTexCubeIndex);
    }

    Textures wallTextures = CreateTextures("bricks2.jpg", "", "bricks2_normal.jpg", "bricks2_disp.jpg");
    for (u32 wallIndex = 0; wallIndex < NUM_OBJECTS; wallIndex++)
    {
        u32 curWallIndex = AddObject(transientInfo, transientInfo->mainQuadVao, 6, CreateRandomVec3(), &wallTextures);
        AddObjectToShaderPass(&transientInfo->dirDepthMapShader, curWallIndex);
        AddObjectToShaderPass(&transientInfo->spotDepthMapShader, curWallIndex);
        AddObjectToShaderPass(&transientInfo->pointDepthMapShader, curWallIndex);
        AddObjectToShaderPass(&transientInfo->gBufferShader, curWallIndex);
        AddObjectToShaderPass(&transientInfo->ssaoShader, curWallIndex);
        AddObjectToShaderPass(&transientInfo->ssaoBlurShader, curWallIndex);
    }

    drawingInfo->dirLight.direction =
        glm::normalize(pointLights[NUM_POINTLIGHTS - 1].position - pointLights[0].position);

    LoadDrawingInfo(transientInfo, drawingInfo, cameraInfo);

    CreateFramebuffers(window, transientInfo);

    CreateSkybox(transientInfo);
    glDepthFunc(GL_LEQUAL); // All skybox points are given a depth of 1.f.

    glGenBuffers(1, &transientInfo->matricesUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, transientInfo->matricesUBO);
    char label[] = "UBO: matrices";
    glObjectLabel(GL_BUFFER, transientInfo->matricesUBO, -1, label);
    glBufferData(GL_UNIFORM_BUFFER, 10 * sizeof(glm::mat4), NULL, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, transientInfo->matricesUBO);

    GenerateSSAOSamplesAndNoise(transientInfo);

    drawingInfo->initialized = true;

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

internal void SaveShaderPass(FILE *file, ShaderProgram *shader)
{
    fwrite(&shader->numObjects, sizeof(u32), 1, file);
    fwrite(shader->objectIndices, sizeof(u32), shader->numObjects, file);
    fwrite(&shader->numModels, sizeof(u32), 1, file);
    fwrite(shader->modelIndices, sizeof(u32), shader->numModels, file);
}

extern "C" __declspec(dllexport) void SaveDrawingInfo(TransientDrawingInfo *transientInfo, PersistentDrawingInfo *info,
                                                      CameraInfo *cameraInfo)
{
    FILE *file;
    fopen_s(&file, "save.bin", "wb");

    info->numObjects = transientInfo->numObjects;
    for (u32 i = 0; i < transientInfo->numObjects; i++)
    {
        info->objectPositions[i] = transientInfo->objects[i].position;
    }
    info->numModels = transientInfo->numModels;
    for (u32 i = 0; i < transientInfo->numModels; i++)
    {
        info->modelPositions[i] = transientInfo->models[i].position;
    }
    fwrite(info, sizeof(PersistentDrawingInfo), 1, file);
    SaveShaderPass(file, &transientInfo->gBufferShader);
    SaveShaderPass(file, &transientInfo->ssaoShader);
    SaveShaderPass(file, &transientInfo->ssaoBlurShader);
    SaveShaderPass(file, &transientInfo->dirDepthMapShader);
    SaveShaderPass(file, &transientInfo->spotDepthMapShader);
    SaveShaderPass(file, &transientInfo->pointDepthMapShader);
    SaveShaderPass(file, &transientInfo->instancedObjectShader);
    SaveShaderPass(file, &transientInfo->colorShader);
    SaveShaderPass(file, &transientInfo->outlineShader);
    SaveShaderPass(file, &transientInfo->glassShader);
    SaveShaderPass(file, &transientInfo->textureShader);
    SaveShaderPass(file, &transientInfo->postProcessShader);
    SaveShaderPass(file, &transientInfo->skyboxShader);
    SaveShaderPass(file, &transientInfo->geometryShader);
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

    return glm::normalize(glm::vec3(cameraForwardVec));
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
    if (HasNewVersion(&info->gBufferShader) || HasNewVersion(&info->ssaoShader) ||
        HasNewVersion(&info->ssaoBlurShader) || HasNewVersion(&info->nonPointLightingShader) ||
        HasNewVersion(&info->pointLightingShader) || HasNewVersion(&info->dirDepthMapShader) ||
        HasNewVersion(&info->spotDepthMapShader) || HasNewVersion(&info->pointDepthMapShader) ||
        HasNewVersion(&info->instancedObjectShader) || HasNewVersion(&info->colorShader) ||
        HasNewVersion(&info->outlineShader) || HasNewVersion(&info->glassShader) ||
        HasNewVersion(&info->textureShader) || HasNewVersion(&info->postProcessShader) ||
        HasNewVersion(&info->skyboxShader) || HasNewVersion(&info->geometryShader) ||
        HasNewVersion(&info->gaussianShader))
    {
        bool reloadedShaders = CreateShaderPrograms(info);
        myAssert(reloadedShaders);
    }
}

internal void RenderObject(Object *object, u32 shaderProgram, f32 yRot = 0.f, float scale = 1.f)
{
    glBindVertexArray(object->VAO);

    glBindTextureUnit(10, object->textures.diffuse.id);
    glBindTextureUnit(11, object->textures.specular.id);
    glBindTextureUnit(12, object->textures.normals.id);
    glBindTextureUnit(13, object->textures.displacement.id);
    SetShaderUniformInt(shaderProgram, "displace", object->textures.displacement.id > 0);

    // Model matrix: transforms vertices from local to world space.
    glm::mat4 modelMatrix = glm::mat4(1.f);
    modelMatrix = glm::translate(modelMatrix, object->position);
    modelMatrix = glm::rotate(modelMatrix, yRot, glm::vec3(0.f, 1.f, 0.f));
    modelMatrix = glm::scale(modelMatrix, glm::vec3(scale));
    glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(modelMatrix)));

    SetShaderUniformMat4(shaderProgram, "modelMatrix", &modelMatrix);
    SetShaderUniformMat3(shaderProgram, "normalMatrix", &normalMatrix);
    glDrawElements(GL_TRIANGLES, object->numIndices, GL_UNSIGNED_INT, 0);
}

internal void RenderWithColorShader(TransientDrawingInfo *transientInfo, PersistentDrawingInfo *persistentInfo)
{
    u32 shaderProgram = transientInfo->colorShader.id;
    glUseProgram(shaderProgram);

    glBindVertexArray(transientInfo->cubeVao);

    for (u32 lightIndex = 0; lightIndex < NUM_POINTLIGHTS; lightIndex++)
    {
        PointLight *curLight = &persistentInfo->pointLights[lightIndex];

        glEnable(GL_STENCIL_TEST);
        glStencilMask(0xff);
        glClear(GL_STENCIL_BUFFER_BIT);
        glStencilFunc(GL_ALWAYS, 1, 0xff);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

        SetShaderUniformVec3(shaderProgram, "color", curLight->diffuse);
        Object lightObject = {transientInfo->cubeVao, 36, curLight->position};
        RenderObject(&lightObject, shaderProgram, 0.f, .1f);

        glStencilMask(0x00);
        glStencilFunc(GL_NOTEQUAL, 1, 0xff);

        glm::vec4 stencilColor = glm::vec4(0.f, 0.f, 1.f, 1.f);
        SetShaderUniformVec3(shaderProgram, "color", stencilColor);
        RenderObject(&lightObject, shaderProgram, 0.f, .11f);

        glDisable(GL_STENCIL_TEST);
    }
}

enum class RenderPassType
{
    Normal,
    DirShadowMap,
    SpotShadowMap,
    PointShadowMap
};

void DrawScene(CameraInfo *cameraInfo, TransientDrawingInfo *transientInfo, PersistentDrawingInfo *persistentInfo,
               u32 fbo, HWND window, Arena *listArena, Arena *tempArena, bool dynamicEnvPass = false,
               RenderPassType passType = RenderPassType::Normal);

internal void RenderModel(Model *model, u32 shaderProgram, u32 skyboxTexture = 0, u32 numInstances = 1)
{
    glUseProgram(shaderProgram);

    for (u32 i = 0; i < model->meshCount; i++)
    {
        glBindVertexArray(model->vaos[i]);

        Mesh *mesh = &model->meshes[i];
        u32 textureSlot = 0;
        for (; textureSlot < mesh->numTextures; textureSlot++)
        {
            glBindTextureUnit(10 + textureSlot, mesh->textures[textureSlot].id);
        }
        SetShaderUniformInt(shaderProgram, "displace", textureSlot == 4);
        if (textureSlot < 4)
        {
            glBindTextureUnit(13, 0);
        }

        if (numInstances == 1)
        {
            // Model matrix: transforms vertices from local to world space.
            glm::mat4 modelMatrix = glm::mat4(1.f);
            modelMatrix = glm::translate(modelMatrix, model->position);
            modelMatrix = glm::scale(modelMatrix, model->scale);

            modelMatrix *= mesh->relativeTransform;

            glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(modelMatrix)));

            SetShaderUniformMat4(shaderProgram, "modelMatrix", &modelMatrix);
            SetShaderUniformMat3(shaderProgram, "normalMatrix", &normalMatrix);

            glDrawElements(GL_TRIANGLES, mesh->indicesSize / sizeof(u32), GL_UNSIGNED_INT, 0);
        }
        else
        {
            SetShaderUniformFloat(shaderProgram, "time", Win32GetTime());
            glDrawElementsInstanced(GL_TRIANGLES, mesh->indicesSize / sizeof(u32), GL_UNSIGNED_INT, 0, numInstances);
        }
    }
}

internal void RenderShaderPass(ShaderProgram *shaderProgram, TransientDrawingInfo *transientInfo)
{
    glUseProgram(shaderProgram->id);

    for (u32 i = 0; i < shaderProgram->numObjects; i++)
    {
        u32 curIndex = shaderProgram->objectIndices[i];
        Object *curObject = &transientInfo->objects[curIndex];
        RenderObject(curObject, shaderProgram->id);
    }
    for (u32 i = 0; i < shaderProgram->numModels; i++)
    {
        u32 curIndex = shaderProgram->modelIndices[i];
        RenderModel(&transientInfo->models[curIndex], shaderProgram->id);
    }
}

internal void FlipImage(u8 *data, s32 width, s32 height, u32 bytesPerPixel, Arena *tempArena)
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

internal void SetGBufferUniforms(u32 shaderProgram, PersistentDrawingInfo *persistentInfo, CameraInfo *cameraInfo)
{
    glUseProgram(shaderProgram);

    SetShaderUniformFloat(shaderProgram, "shininess", persistentInfo->materialShininess);
    SetShaderUniformVec3(shaderProgram, "cameraPos", cameraInfo->pos);
    SetShaderUniformFloat(shaderProgram, "heightScale", .1f);
}

internal void FillGBuffer(CameraInfo *cameraInfo, TransientDrawingInfo *transientInfo,
                          PersistentDrawingInfo *persistentInfo)
{
    u32 shaderProgram = transientInfo->gBufferShader.id;
    SetGBufferUniforms(shaderProgram, persistentInfo, cameraInfo);

    RenderShaderPass(&transientInfo->gBufferShader, transientInfo);
}

internal glm::vec3 GetCameraUpVector(CameraInfo *cameraInfo)
{
    glm::vec3 cameraForwardVec = GetCameraForwardVector(cameraInfo);
    glm::vec3 cameraTarget = cameraInfo->pos + cameraForwardVec;

    glm::vec3 cameraDirection = glm::normalize(cameraInfo->pos - cameraTarget);

    glm::vec3 upVector = glm::vec3(0.f, 1.f, 0.f);
    glm::vec3 cameraRightVec = glm::normalize(glm::cross(upVector, cameraDirection));
    return glm::normalize(glm::cross(cameraDirection, cameraRightVec));
}

internal void GetPerspectiveRenderingMatrices(CameraInfo *cameraInfo, glm::mat4 *outViewMatrix,
                                              glm::mat4 *outProjectionMatrix)
{
    glm::vec3 cameraTarget = cameraInfo->pos + GetCameraForwardVector(cameraInfo);
    glm::vec3 cameraUpVec = GetCameraUpVector(cameraInfo);
    f32 nearPlaneDistance = .1f;
    f32 farPlaneDistance = 150.f;
    *outProjectionMatrix =
        glm::perspective(glm::radians(cameraInfo->fov), cameraInfo->aspectRatio, nearPlaneDistance, farPlaneDistance);
    *outViewMatrix = LookAt(cameraInfo, cameraTarget, cameraUpVec, farPlaneDistance);
}

internal void SetLightingShaderUniforms(CameraInfo *cameraInfo, TransientDrawingInfo *transientInfo,
                                        PersistentDrawingInfo *persistentInfo, bool rearView)
{
    glBindTextureUnit(10, rearView ? transientInfo->rearViewQuads[0] : transientInfo->mainQuads[0]);
    glBindTextureUnit(11, rearView ? transientInfo->rearViewQuads[1] : transientInfo->mainQuads[1]);
    glBindTextureUnit(12, rearView ? transientInfo->rearViewQuads[2] : transientInfo->mainQuads[2]);
    glBindTextureUnit(13, transientInfo->ssaoQuad);
    glBindTextureUnit(14, transientInfo->skyboxTexture);
    glBindTextureUnit(15, transientInfo->dirShadowMapQuad);
    glBindTextureUnit(16, transientInfo->spotShadowMapQuad);

    u32 nonPointShader = transientInfo->nonPointLightingShader.id;
    glUseProgram(nonPointShader);

    SetShaderUniformFloat(nonPointShader, "shininess", persistentInfo->materialShininess);
    SetShaderUniformInt(nonPointShader, "blinn", persistentInfo->blinn);

    SetShaderUniformVec3(nonPointShader, "dirLight.direction", persistentInfo->dirLight.direction);
    SetShaderUniformVec3(nonPointShader, "dirLight.ambient", persistentInfo->dirLight.ambient);
    SetShaderUniformVec3(nonPointShader, "dirLight.diffuse", persistentInfo->dirLight.diffuse);
    SetShaderUniformVec3(nonPointShader, "dirLight.specular", persistentInfo->dirLight.specular);

    SetShaderUniformVec3(nonPointShader, "spotLight.position", cameraInfo->pos);
    SetShaderUniformVec3(nonPointShader, "spotLight.direction", GetCameraForwardVector(cameraInfo));
    SetShaderUniformFloat(nonPointShader, "spotLight.innerCutoff", cosf(persistentInfo->spotLight.innerCutoff));
    SetShaderUniformFloat(nonPointShader, "spotLight.outerCutoff", cosf(persistentInfo->spotLight.outerCutoff));
    SetShaderUniformVec3(nonPointShader, "spotLight.ambient", persistentInfo->spotLight.ambient);
    SetShaderUniformVec3(nonPointShader, "spotLight.diffuse", persistentInfo->spotLight.diffuse);
    SetShaderUniformVec3(nonPointShader, "spotLight.specular", persistentInfo->spotLight.specular);

    SetShaderUniformVec3(nonPointShader, "cameraPos", cameraInfo->pos);

    SetShaderUniformFloat(nonPointShader, "heightScale", .1f);

    u32 pointShader = transientInfo->pointLightingShader.id;
    glUseProgram(pointShader);
    SetShaderUniformInt(pointShader, "blinn", persistentInfo->blinn);
    SetShaderUniformVec3(pointShader, "cameraPos", cameraInfo->pos);
}

internal void RenderQuad(TransientDrawingInfo *transientInfo, u32 shaderProgram)
{
    glm::mat4 identity = glm::mat4(1.f);
    glBindBuffer(GL_UNIFORM_BUFFER, transientInfo->matricesUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, 64, &identity);
    glBufferSubData(GL_UNIFORM_BUFFER, 64, 64, &identity);

    glBindVertexArray(transientInfo->mainQuadVao);
    glUseProgram(shaderProgram);
    SetShaderUniformMat4(shaderProgram, "modelMatrix", &identity);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

internal void ExecuteLightingPass(CameraInfo *cameraInfo, TransientDrawingInfo *transientInfo,
                                  PersistentDrawingInfo *persistentInfo, HWND window, Arena *listArena,
                                  Arena *tempArena, bool rearView, bool dynamicEnvPass = false)
{
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, rearView ? "Rear-view lighting pass" : "Main lighting pass");

    // TODO: find a proper way to parameterize dynamic environment mapping. We only want certain
    // objects to reflect the environment, not all of them. It should depend on their shininess.
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
            glObjectLabel(GL_TEXTURE, dynamicEnvMap.skyboxTexture, -1, "Texture (cube map): dynamic environment map");
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
            glBindBuffer(GL_UNIFORM_BUFFER, transientInfo->matricesUBO);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, 64, &savedViewMatrix);
            glBufferSubData(GL_UNIFORM_BUFFER, 64, 64, &savedProjectionMatrix);
        }
        else
        {
            return;
        }
    }
#endif

    glBindFramebuffer(GL_FRAMEBUFFER, rearView ? transientInfo->rearViewLightingFBO : transientInfo->lightingFBO);
    GLenum attachments[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, attachments);
    glClearColor(1.f, 1.f, 1.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Non-point lighting.
    u32 nonPointShaderProgram = transientInfo->nonPointLightingShader.id;
    glUseProgram(nonPointShaderProgram);
    SetLightingShaderUniforms(cameraInfo, transientInfo, persistentInfo, rearView);

    RenderQuad(transientInfo, nonPointShaderProgram);

    // Point lighting.
    // NOTE: perhaps we could do instanced rendering of the spheres instead?
    u32 pointShader = transientInfo->pointLightingShader.id;
    glUseProgram(pointShader);

    glm::mat4 viewMatrix, projectionMatrix;
    GetPerspectiveRenderingMatrices(cameraInfo, &viewMatrix, &projectionMatrix);
    glBindBuffer(GL_UNIFORM_BUFFER, transientInfo->matricesUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, 64, &viewMatrix);
    glBufferSubData(GL_UNIFORM_BUFFER, 64, 64, &projectionMatrix);

    RECT clientRect;
    GetClientRect(window, &clientRect);
    s32 width = clientRect.right;
    s32 height = clientRect.bottom;

    glm::vec2 screenSize{(f32)width, (f32)height};
    SetShaderUniformVec2(pointShader, "screenSize", screenSize);

    glEnable(GL_STENCIL_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, rearView ? transientInfo->rearViewFBO : transientInfo->mainFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, rearView ? transientInfo->rearViewLightingFBO : transientInfo->lightingFBO);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glDrawBuffers(2, attachments);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    for (u32 lightIndex = 0; lightIndex < NUM_POINTLIGHTS; lightIndex++)
    {
        PointLight *lights = persistentInfo->pointLights;
        PointLight light = lights[lightIndex];
        Attenuation *att = &globalAttenuationTable[light.attIndex];

        glColorMask(0, 0, 0, 0);

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);

        glStencilMask(0xff);
        glClear(GL_STENCIL_BUFFER_BIT);
        glStencilFunc(GL_ALWAYS, 0, 0);
        glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP, GL_KEEP);
        glStencilOpSeparate(GL_BACK, GL_KEEP, GL_INCR_WRAP, GL_KEEP);

        Model *model = transientInfo->sphereModel;
        for (u32 i = 0; i < model->meshCount; i++)
        {
            glBindVertexArray(model->vaos[i]);

            Mesh *mesh = &model->meshes[i];
            glm::mat4 lightModelMatrix = glm::mat4(1.f);
            lightModelMatrix = glm::translate(lightModelMatrix, light.position);
            f32 constant = 1.f;
            f32 linear = att->linear;
            f32 quadratic = att->quadratic;
            f32 lightMax = fmax(fmax(light.diffuse.r, light.diffuse.g), light.diffuse.b);
            f32 radius = (-linear + sqrtf(linear * linear - 4.f * quadratic * (constant - (256.f / 5.f) * lightMax))) /
                         (2.f * quadratic);
            lightModelMatrix = glm::scale(lightModelMatrix, glm::vec3(radius));
            lightModelMatrix *= mesh->relativeTransform;
            glm::mat3 lightNormalMatrix = glm::mat3(glm::transpose(glm::inverse(lightModelMatrix)));

            SetShaderUniformMat4(pointShader, "modelMatrix", &lightModelMatrix);
            SetShaderUniformMat3(pointShader, "normalMatrix", &lightNormalMatrix);

            glDrawElements(GL_TRIANGLES, mesh->indicesSize / sizeof(u32), GL_UNSIGNED_INT, 0);
        }

        glColorMask(0xff, 0xff, 0xff, 0xff);

        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);

        glStencilMask(0x00);
        glStencilFunc(GL_NOTEQUAL, 0, 0xff);

        SetShaderUniformVec3(pointShader, "pointLight.position", light.position);
        SetShaderUniformVec3(pointShader, "pointLight.ambient", light.ambient);
        SetShaderUniformVec3(pointShader, "pointLight.diffuse", light.diffuse);
        SetShaderUniformVec3(pointShader, "pointLight.specular", light.specular);

        SetShaderUniformFloat(pointShader, "pointLight.linear", att->linear);
        SetShaderUniformFloat(pointShader, "pointLight.quadratic", att->quadratic);

        glBindTextureUnit(17, transientInfo->pointShadowMapQuad[lightIndex]);

        for (u32 i = 0; i < model->meshCount; i++)
        {
            Mesh *mesh = &model->meshes[i];
            glDrawElements(GL_TRIANGLES, mesh->indicesSize / sizeof(u32), GL_UNSIGNED_INT, 0);
        }

        glCullFace(GL_BACK);
        glDisable(GL_CULL_FACE);
    }

    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);

    glPopDebugGroup();
}

internal void RenderWithGeometryShader(TransientDrawingInfo *transientInfo)
{
    u32 shaderProgram = transientInfo->geometryShader.id;
    glUseProgram(shaderProgram);

    SetShaderUniformVec3(shaderProgram, "color", glm::vec3(1.f, 1.f, 0.f));

    RenderShaderPass(&transientInfo->geometryShader, transientInfo);
}

internal void RenderWithTextureShader(CameraInfo *cameraInfo, TransientDrawingInfo *transientInfo,
                                      PersistentDrawingInfo *persistentInfo)

{
    u32 shaderProgram = transientInfo->textureShader.id;
    glUseProgram(shaderProgram);

    SetShaderUniformVec3(shaderProgram, "cameraPos", cameraInfo->pos);

    glEnable(GL_CULL_FACE);
    RenderShaderPass(&transientInfo->textureShader, transientInfo);
    glDisable(GL_CULL_FACE);
}

internal void RenderWithGlassShader(CameraInfo *cameraInfo, TransientDrawingInfo *transientInfo,
                                    PersistentDrawingInfo *persistentInfo, Arena *listArena, Arena *tempArena)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    u32 shaderProgram = transientInfo->glassShader.id;
    glUseProgram(shaderProgram);

    glBindTextureUnit(11, transientInfo->skyboxTexture);

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

internal void GetPassTypeAsString(RenderPassType type, u32 bufSize, char *outString)
{
    switch (type)
    {
    case RenderPassType::Normal:
        sprintf_s(outString, bufSize, "Geometry pass");
        return;
    case RenderPassType::DirShadowMap:
        sprintf_s(outString, bufSize, "Directional shadow map pass");
        return;
    case RenderPassType::SpotShadowMap:
        sprintf_s(outString, bufSize, "Spot shadow map pass");
        return;
    case RenderPassType::PointShadowMap:
        sprintf_s(outString, bufSize, "Point shadow map pass");
        return;
    }
    myAssert(false);
}

void DrawScene(CameraInfo *cameraInfo, TransientDrawingInfo *transientInfo, PersistentDrawingInfo *persistentInfo,
               u32 fbo, HWND window, Arena *listArena, Arena *tempArena, bool dynamicEnvPass, RenderPassType passType)
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    char passTypeAsString[32];
    GetPassTypeAsString(passType, 32, passTypeAsString);
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, passTypeAsString);

    // NOTE: SSAO shader relies on position buffer background being (0, 0, 0).
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glStencilMask(0xff);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glStencilMask(0x00);

    // TODO: move these calculations out since they don't depend on camera placement so we don't
    // need to do them every time the scene is drawn? Also it means the buffer data gets overwritten
    // in subsequent passes, eg for point shadow mapping and skybox rendering, where the FOV is set to
    // 90 degrees.
    f32 dirLightNearPlaneDistance = 1.f;
    f32 dirLightFarPlaneDistance = 7.5f;
    glm::vec3 dirEye = glm::vec3(0.f) - glm::normalize(persistentInfo->dirLight.direction) * 5.f;
    glm::mat4 dirLightViewMatrix = glm::lookAt(dirEye, glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f));
    glm::mat4 dirLightProjectionMatrix =
        glm::ortho(-10.f, 10.f, -10.f, 10.f, dirLightNearPlaneDistance, dirLightFarPlaneDistance);
    glm::mat4 dirLightSpaceMatrix = dirLightProjectionMatrix * dirLightViewMatrix;

    glm::mat4 viewMatrix, projectionMatrix;
    GetPerspectiveRenderingMatrices(cameraInfo, &viewMatrix, &projectionMatrix);

    glm::vec3 spotEye = cameraInfo->pos;
    glm::mat4 spotLightViewMatrix =
        glm::lookAt(spotEye, spotEye + GetCameraForwardVector(cameraInfo), GetCameraUpVector(cameraInfo));
    glm::mat4 spotLightSpaceMatrix = projectionMatrix * spotLightViewMatrix;

    glBindBuffer(GL_UNIFORM_BUFFER, transientInfo->matricesUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, 64, &viewMatrix);
    glBufferSubData(GL_UNIFORM_BUFFER, 64, 64, &projectionMatrix);
    glBufferSubData(GL_UNIFORM_BUFFER, 128, 64, &dirLightSpaceMatrix);
    glBufferSubData(GL_UNIFORM_BUFFER, 192, 64, &spotLightSpaceMatrix);

    if (passType == RenderPassType::DirShadowMap)
    {
        RenderShaderPass(&transientInfo->dirDepthMapShader, transientInfo);
    }
    else if (passType == RenderPassType::SpotShadowMap)
    {
        RenderShaderPass(&transientInfo->spotDepthMapShader, transientInfo);
    }
    else if (passType == RenderPassType::PointShadowMap)
    {
        RenderShaderPass(&transientInfo->pointDepthMapShader, transientInfo);
    }
    else
    {
        // Deferred skybox rendering is achieved thus:
        // 1. When drawing geometry, set stencil value to 1.
        // 2. Execute lighting pass on geometry.
        // 3. Render skybox where stencil value is 0.
        glEnable(GL_STENCIL_TEST);
        glStencilMask(0xff);
        glStencilFunc(GL_ALWAYS, 1, 0xff);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

        // G-buffer pass.
        FillGBuffer(cameraInfo, transientInfo, persistentInfo);

        glDisable(GL_STENCIL_TEST);

        // RenderWithGeometryShader(transientInfo);

        // Textured cubes.
        // RenderWithTextureShader(cameraInfo, transientInfo, persistentInfo);

        // Windows.
        // RenderWithGlassShader(cameraInfo, transientInfo, persistentInfo, listArena, tempArena);
    }

    glPopDebugGroup();
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
    ImGui::SliderFloat("Exposure", &persistentInfo->exposure, 0.f, 5.f);
    ImGui::SliderFloat("SSAO sampling radius", &persistentInfo->ssaoSamplingRadius, 0.f, 1.f);
    ImGui::SliderFloat("SSAO power", &persistentInfo->ssaoPower, 0.f, 8.f);

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

    if (ImGui::CollapsingHeader("Positions"))
    {
        for (u32 i = 0; i < transientInfo->numModels; i++)
        {
            ImGui::PushID(id++);
            ImGui::SliderFloat3("Position", glm::value_ptr(transientInfo->models[i].position), -10.f, 10.f);
            ImGui::PopID();
        }
        for (u32 i = 0; i < transientInfo->numObjects; i++)
        {
            ImGui::PushID(id++);
            ImGui::SliderFloat3("Position", glm::value_ptr(transientInfo->objects[i].position), -10.f, 10.f);
            ImGui::PopID();
        }
    }

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

internal void DrawSkybox(TransientDrawingInfo *transientInfo, CameraInfo *cameraInfo, s32 width, s32 height,
                         bool rearView)
{
    // Draw skybox where geometry rendering pass did not set stencil value to 1.
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, rearView ? "Rear-view skybox pass" : "Main skybox pass");

    glBindFramebuffer(GL_READ_FRAMEBUFFER, rearView ? transientInfo->rearViewFBO : transientInfo->mainFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, rearView ? transientInfo->rearViewLightingFBO : transientInfo->lightingFBO);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_STENCIL_BUFFER_BIT, GL_NEAREST);

    glEnable(GL_STENCIL_TEST);
    glStencilMask(0x00);
    glStencilFunc(GL_NOTEQUAL, 1, 0xff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    u32 shaderProgram = transientInfo->skyboxShader.id;
    glUseProgram(shaderProgram);

    glm::mat4 viewMatrix, projectionMatrix;
    GetPerspectiveRenderingMatrices(cameraInfo, &viewMatrix, &projectionMatrix);
    glm::mat4 skyboxViewMatrix = glm::mat4(glm::mat3(viewMatrix));
    glBufferSubData(GL_UNIFORM_BUFFER, 0, 64, &skyboxViewMatrix);
    glBufferSubData(GL_UNIFORM_BUFFER, 64, 64, &projectionMatrix);

    glBindVertexArray(transientInfo->cubeVao);
    glBindTextureUnit(10, transientInfo->skyboxTexture);

    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

    glDisable(GL_STENCIL_TEST);

    glPopDebugGroup();
}

internal void ExecuteSSAOPass(TransientDrawingInfo *transientInfo, PersistentDrawingInfo *persistentInfo,
                              CameraInfo *cameraInfo, glm::vec2 screenSize, bool rearView)
{
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, rearView ? "Rear-view SSAO pass" : "Main SSAO pass");

    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Sampling subpass");
    glBindFramebuffer(GL_FRAMEBUFFER, transientInfo->ssaoFBO);
    glClear(GL_COLOR_BUFFER_BIT);

    u32 shaderProgram = transientInfo->ssaoShader.id;
    glUseProgram(shaderProgram);
    glBindTextureUnit(10, transientInfo->mainQuads[0]);
    glBindTextureUnit(11, transientInfo->mainQuads[1]);
    glBindTextureUnit(12, transientInfo->ssaoNoiseTexture);
    glm::mat4 cameraViewMatrix, cameraProjectionMatrix;
    GetPerspectiveRenderingMatrices(cameraInfo, &cameraViewMatrix, &cameraProjectionMatrix);
    SetShaderUniformMat4(shaderProgram, "cameraViewMatrix", &cameraViewMatrix);
    SetShaderUniformMat4(shaderProgram, "cameraProjectionMatrix", &cameraProjectionMatrix);
    SetShaderUniformVec2(shaderProgram, "screenSize", screenSize);
    SetShaderUniformFloat(shaderProgram, "radius", persistentInfo->ssaoSamplingRadius);
    SetShaderUniformFloat(shaderProgram, "power", persistentInfo->ssaoPower);
    RenderQuad(transientInfo, shaderProgram);

    glPopDebugGroup();

    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Noise subpass");

    glBindFramebuffer(GL_FRAMEBUFFER, transientInfo->ssaoBlurFBO);
    glClear(GL_COLOR_BUFFER_BIT);

    shaderProgram = transientInfo->ssaoBlurShader.id;
    glUseProgram(shaderProgram);
    glBindTextureUnit(10, transientInfo->ssaoQuad);
    RenderQuad(transientInfo, shaderProgram);

    glPopDebugGroup();

    glPopDebugGroup();
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

    RECT clientRect;
    GetClientRect(window, &clientRect);
    s32 width = clientRect.right;
    s32 height = clientRect.bottom;

    // Directional shadow map pass.
    // NOTE: front-face culling is a sledgehammer solution to Peter-Panning and may break down with
    // some objects.
    glViewport(0, 0, DIR_SHADOW_MAP_SIZE, DIR_SHADOW_MAP_SIZE);
    glCullFace(GL_FRONT);
    DrawScene(cameraInfo, transientInfo, persistentInfo, transientInfo->dirShadowMapFBO, window, listArena, tempArena,
              false, RenderPassType::DirShadowMap);

    // Spot shadow map pass.
    DrawScene(cameraInfo, transientInfo, persistentInfo, transientInfo->spotShadowMapFBO, window, listArena, tempArena,
              false, RenderPassType::SpotShadowMap);
    glCullFace(GL_BACK);

    // Point lights shadow map pass.
    glViewport(0, 0, POINT_SHADOW_MAP_SIZE, POINT_SHADOW_MAP_SIZE);
    f32 pointAspectRatio = 1.f;
    f32 pointNear = .1f;
    f32 pointFar = 50.f; // TODO: make this depend on the attenuation.
    glm::mat4 pointShadowProjection = glm::perspective(glm::radians(90.f), pointAspectRatio, pointNear, pointFar);
    for (u32 i = 0; i < NUM_POINTLIGHTS; i++)
    {
        glm::mat4 pointShadowMatrices[6];
        CameraInfo pointCameraInfo = *cameraInfo;
        pointCameraInfo.pos = persistentInfo->pointLights[i].position;
        pointCameraInfo.fov = PI / 2.f;
        pointShadowMatrices[0] =
            pointShadowProjection * LookAt(&pointCameraInfo, pointCameraInfo.pos + glm::vec3(1.f, 0.f, 0.f),
                                           glm::vec3(0.f, -1.f, 0.f), pointFar);
        pointShadowMatrices[1] =
            pointShadowProjection * LookAt(&pointCameraInfo, pointCameraInfo.pos + glm::vec3(-1.f, 0.f, 0.f),
                                           glm::vec3(0.f, -1.f, 0.f), pointFar);
        pointShadowMatrices[2] =
            pointShadowProjection * LookAt(&pointCameraInfo, pointCameraInfo.pos + glm::vec3(0.f, 1.f, 0.f),
                                           glm::vec3(0.f, 0.f, 1.f), pointFar);
        pointShadowMatrices[3] =
            pointShadowProjection * LookAt(&pointCameraInfo, pointCameraInfo.pos + glm::vec3(0.f, -1.f, 0.f),
                                           glm::vec3(0.f, 0.f, -1.f), pointFar);
        pointShadowMatrices[4] =
            pointShadowProjection * LookAt(&pointCameraInfo, pointCameraInfo.pos + glm::vec3(0.f, 0.f, 1.f),
                                           glm::vec3(0.f, -1.f, 0.f), pointFar);
        pointShadowMatrices[5] =
            pointShadowProjection * LookAt(&pointCameraInfo, pointCameraInfo.pos + glm::vec3(0.f, 0.f, -1.f),
                                           glm::vec3(0.f, -1.f, 0.f), pointFar);

        glBindBuffer(GL_UNIFORM_BUFFER, transientInfo->matricesUBO);
        for (u32 j = 0; j < 6; j++)
        {
            glBufferSubData(GL_UNIFORM_BUFFER, 256 + j * 64, 64, &pointShadowMatrices[j]);
        }
        u32 pointShaderProgram = transientInfo->pointDepthMapShader.id;
        glUseProgram(pointShaderProgram);
        SetShaderUniformVec3(pointShaderProgram, "lightPos", pointCameraInfo.pos);
        SetShaderUniformFloat(pointShaderProgram, "farPlane", pointFar);
        u32 lightingShaderProgram = transientInfo->pointLightingShader.id;
        glUseProgram(lightingShaderProgram);
        SetShaderUniformFloat(lightingShaderProgram, "pointFar", pointFar);
        u32 instancedObjectShaderProgram = transientInfo->instancedObjectShader.id;
        glUseProgram(instancedObjectShaderProgram);
        SetShaderUniformFloat(instancedObjectShaderProgram, "pointFar", pointFar);

        DrawScene(&pointCameraInfo, transientInfo, persistentInfo, transientInfo->pointShadowMapFBO[i], window,
                  listArena, tempArena, false, RenderPassType::PointShadowMap);
    }

    glViewport(0, 0, width, height);
    // Main pass.
    DrawScene(cameraInfo, transientInfo, persistentInfo, transientInfo->mainFBO, window, listArena, tempArena);

    CameraInfo rearViewCamera = *cameraInfo;
    rearViewCamera.yaw += PI;
    rearViewCamera.pitch *= -1.f;
    rearViewCamera.forwardVector = GetCameraForwardVector(&rearViewCamera);
    rearViewCamera.rightVector = GetCameraRightVector(&rearViewCamera);

    // Rear-view pass.
    DrawScene(&rearViewCamera, transientInfo, persistentInfo, transientInfo->rearViewFBO, window, listArena, tempArena);

    glDisable(GL_DEPTH_TEST);

    // Main SSAO pass.
    glm::vec2 screenSize(width, height);
    ExecuteSSAOPass(transientInfo, persistentInfo, cameraInfo, screenSize, false);

    // Main lighting pass.
    ExecuteLightingPass(cameraInfo, transientInfo, persistentInfo, window, listArena, tempArena, false);

    // Rear-view lighting pass.
    ExecuteLightingPass(cameraInfo, transientInfo, persistentInfo, window, listArena, tempArena, true);

    // Main skybox pass.
    DrawSkybox(transientInfo, cameraInfo, width, height, false);

    // Rear-view skybox pass.
    DrawSkybox(transientInfo, &rearViewCamera, width, height, true);

    // Apply Gaussian blur to brightness texture to generate bloom.
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Gaussian blur for bloom");
    bool horizontal = true;
    u32 gaussianShader = transientInfo->gaussianShader.id;
    u32 gaussianQuad = transientInfo->lightingQuads[1];
    glUseProgram(gaussianShader);
    for (u32 i = 0; i < 10; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, transientInfo->gaussianFBOs[horizontal]);
        SetShaderUniformInt(gaussianShader, "horizontal", horizontal);
        glBindTextureUnit(10, gaussianQuad);
        horizontal = !horizontal;

        RenderQuad(transientInfo, gaussianShader);

        gaussianQuad = transientInfo->gaussianQuads[!horizontal];
    }
    glPopDebugGroup();

    // NOTE: in the case of the rear-view mirror, since a multisampled framebuffer cannot be blitted
    // to only a portion of a non-multisampled framebuffer, there are 3 options:
    // 1. blit a non-multisampled framebuffer to a portion of the main buffer;
    // 2. draw the texture attached to a non-multisampled buffer to the screen;
    // 3. draw the texture attached to a multisampled buffer to the screen.
    // I went with option 2 here since it results in less aliasing than option 1 and unlike option 3
    // doesn't require maintaining a second shader to apply the same post-processing effects to the
    // multisampled rear-view framebuffer (see: postprocess_ms.fs).
    // See also: ResizeGLViewport().
    /*
    glBindFramebuffer(GL_READ_FRAMEBUFFER, transientInfo->rearViewFBO);
    glBlitFramebuffer(0, 0, width, height, width / 4, height * 17 / 20, width * 3 / 4, height,
    GL_COLOR_BUFFER_BIT, GL_NEAREST);
    */
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(1.f, 1.f, 1.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    u32 shaderProgram = transientInfo->postProcessShader.id;
    glUseProgram(shaderProgram);

    SetShaderUniformFloat(shaderProgram, "gamma", persistentInfo->gamma);
    SetShaderUniformFloat(shaderProgram, "exposure", persistentInfo->exposure);

    // Main quad.
    {
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Main quad post-processing");

        glBindTextureUnit(10, transientInfo->lightingQuads[0]);
        glBindTextureUnit(11, transientInfo->gaussianQuads[0]);

        RenderQuad(transientInfo, shaderProgram);

        glPopDebugGroup();
    }

    // Rear-view quad.
    {
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Rear-view post-processing");

        glBindTextureUnit(10, transientInfo->rearViewLightingQuads[0]);
        // NOTE: for now we don't bother with bloom in the rear-view mirror, since our bloom blur buffer
        // is of the same size as the main framebuffer.
        glBindTextureUnit(11, 0);

        glBindVertexArray(transientInfo->rearViewQuadVao);
        glm::mat4 identity(1.f);
        SetShaderUniformMat4(shaderProgram, "modelMatrix", &identity);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glPopDebugGroup();
    }

    glEnable(GL_DEPTH_TEST);

    // Point lights.
    // TODO: fix effect of outlining on meshes that appear between the camera and the outlined
    // object.
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Point lights");
    glBindFramebuffer(GL_READ_FRAMEBUFFER, transientInfo->mainFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glm::mat4 viewMatrix, projectionMatrix;
    GetPerspectiveRenderingMatrices(cameraInfo, &viewMatrix, &projectionMatrix);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, 64, &viewMatrix);
    glBufferSubData(GL_UNIFORM_BUFFER, 64, 64, &projectionMatrix);
    RenderWithColorShader(transientInfo, persistentInfo);
    glPopDebugGroup();

    DrawDebugWindow(cameraInfo, transientInfo, persistentInfo);

    if (!SwapBuffers(hdc))
    {
        if (MessageBoxW(window, L"Failed to swap buffers", L"OpenGL error", MB_OK) == S_OK)
        {
            *running = false;
        }
    }
}

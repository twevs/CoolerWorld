#include "common.h"
#include "skiplist.h"

#include "asteroids.cpp"
#include "framebuffer.cpp"
#include "gl.cpp"
#include "mesh.cpp"
#include "save_load.cpp"
#include "shader.cpp"
#include "skybox.cpp"
#include "texture.cpp"

#include "tracy/public/TracyClient.cpp"
#include "tracy/public/tracy/Tracy.hpp"
#include "tracy/public/tracy/TracyOpenGL.hpp"

global_variable ImGuiContext *imGuiContext;
global_variable ImGuiIO *imGuiIO;

global_variable s32 gElemCounts[] = {3, 3, 2};                // Position, normal, texcoords.
global_variable s32 gBitangentElemCounts[] = {3, 3, 2, 3, 3}; // Position, normal, texcoords, tangent, bitangent.

// NOTE: for now we put this here as a convenient way to share IDs between models and objects.
// Will no longer be a global if we get rid of that distinction and only use models.
global_variable u32 gObjectId = 0;

// NOTE: current Tracy setup is incompatible with hot reloading.
// See Tracy manual, section "Setup for multi-DLL projects" to fix this.
extern "C" __declspec(dllexport) void InitializeTracyGPUContext()
{
    TracyGpuContext;
}

extern "C" __declspec(dllexport) void InitializeImGuiInModule(HWND window)
{
    IMGUI_CHECKVERSION();
    imGuiContext = ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    imGuiIO = &io;
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(window);
    ImGui_ImplOpenGL3_Init("#version 460");
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

internal void AddCube(TransientDrawingInfo *info, glm::ivec3 position);

extern "C" __declspec(dllexport) void GameHandleClick(TransientDrawingInfo *transientInfo, CWInput button,
                                                      CWPoint coordinates, CWPoint screenSize)
{
    DebugPrintA("Clicked at (%i, %i)\n", coordinates.x, coordinates.y);
    u32 bufSize = 1920 * 1080 * 2;
    Arena *pixelsArena = AllocArena(bufSize);
    u8 *pixels = (u8 *)ArenaPush(pixelsArena, bufSize);
    glGetTextureImage(transientInfo->mainFramebuffer.attachments[3], 0, GL_RG_INTEGER, GL_UNSIGNED_BYTE, bufSize,
                      pixels);
    u32 stride = screenSize.x;
    u16 *pixelsAsRG = (u16 *)pixels;
    u16 pickedPixel = pixelsAsRG[coordinates.x + coordinates.y * stride];
    u32 id = (pickedPixel & 0xff);
    u8 faceInfo = ((pickedPixel & 0xff00) >> 8);
    s8 facingX = ((faceInfo & 0x30) >> 4) - 1;
    s8 facingY = ((faceInfo & 0xc) >> 2) - 1;
    s8 facingZ = ((faceInfo & 0x3)) - 1;
    DebugPrintA("Value: %u, %i, %i, %i\n", id, facingX, facingY, facingZ);
    FreeArena(pixelsArena);

    for (u32 i = 0; i < transientInfo->numObjects; i++)
    {
        Object *obj = &transientInfo->objects[i];
        if (obj->id == id)
        {
            AddCube(transientInfo, obj->position + glm::vec3(facingX, facingY, facingZ));
        }
    }
}

internal u32 AddObject(TransientDrawingInfo *transientInfo, u32 vao, u32 numIndices, glm::vec3 position,
                       Material *textures = nullptr)
{
    u32 objectIndex = transientInfo->numObjects;
    transientInfo->objects[objectIndex] = {gObjectId++, vao, numIndices, position};
    if (textures)
    {
        transientInfo->objects[objectIndex].textures = CreateTextureHandlesFromMaterial(textures);
    }
    transientInfo->numObjects++;
    myAssert(transientInfo->numObjects <= MAX_OBJECTS);
    return objectIndex;
}

// NOTE + TODO: see note in Model struct. There needs to be a clear separation of model geometry data and
// data used to render specific instances of that model with given materials and transforms.
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

internal void LoadModels(TransientDrawingInfo *transientInfo, Arena *texturesArena, Arena *meshDataArena)
{
    u32 backpackIndex = AddModel("backpack.obj", transientInfo, gBitangentElemCounts, myArraySize(gBitangentElemCounts),
                                 texturesArena, meshDataArena, &gObjectId);
    u32 planetIndex = AddModel("planet.obj", transientInfo, gBitangentElemCounts, myArraySize(gBitangentElemCounts),
                               texturesArena, meshDataArena, &gObjectId);
    u32 rockIndex = AddModel("rock.obj", transientInfo, gBitangentElemCounts, myArraySize(gBitangentElemCounts),
                             texturesArena, meshDataArena, &gObjectId);
    u32 deccerCubesIndex = AddModel("SM_Deccer_Cubes_Textured_Complex.fbx", transientInfo, gBitangentElemCounts,
                                    myArraySize(gBitangentElemCounts), texturesArena, meshDataArena, &gObjectId, .01f);
    u32 sphereIndex = AddModel("sphere.fbx", transientInfo, gBitangentElemCounts, myArraySize(gBitangentElemCounts),
                               texturesArena, meshDataArena, &gObjectId);
    transientInfo->sphereModel = &transientInfo->models[sphereIndex];
    // TODO: remove this once per-mesh relative transform is once again accounted for.
    transientInfo->sphereModel->scale = glm::vec3(50.f);

    return;

    /*
    AddModelToShaderPass(&transientInfo->dirDepthMapShader, backpackIndex);
    AddModelToShaderPass(&transientInfo->spotDepthMapShader, backpackIndex);
    AddModelToShaderPass(&transientInfo->pointDepthMapShader, backpackIndex);
    AddModelToShaderPass(&transientInfo->gBufferShader, backpackIndex);
    AddModelToShaderPass(&transientInfo->geometryShader, backpackIndex);
    AddModelToShaderPass(&transientInfo->ssaoShader, backpackIndex);
    AddModelToShaderPass(&transientInfo->ssaoBlurShader, backpackIndex);
    */
}

internal void LoadCube(TransientDrawingInfo *transientInfo, Arena *texturesArena)
{
    FILE *rectFile;
    fopen_s(&rectFile, "rect.bin", "rb");
    // 4 vertices per face * 6 faces * (vec3 pos + vec3 normal + vec2 texCoords + vec3 tangent + vec3 bitangent);
    f32 rectVertices[4 * 6 * (3 + 3 + 2 + 3 + 3)] = {};
    u32 rectIndices[36];
    fread(rectVertices, sizeof(f32), myArraySize(rectVertices), rectFile);
    fread(rectIndices, sizeof(u32), myArraySize(rectIndices), rectFile);
    fclose(rectFile);
    CalculateTangents((Vertex *)rectVertices, 24, rectIndices, 36, texturesArena);

    transientInfo->cubeVao = CreateVAO(rectVertices, sizeof(rectVertices), gBitangentElemCounts,
                                       myArraySize(gBitangentElemCounts), rectIndices, sizeof(rectIndices));
}

internal void AddCube(TransientDrawingInfo *info, glm::ivec3 position)
{
    Material cubeTextures = {};
    cubeTextures.diffuse = CreateTexture("window.png", TextureType::Diffuse, GL_CLAMP_TO_EDGE);
    cubeTextures.normals = CreateTexture("flat_surface_normals.png", TextureType::Normals);

    Cubes *cubes = &info->cubes;
    u32 i = cubes->numCubes;

    cubes->positions[i] = position;
    cubes->textures[i] = CreateTextureHandlesFromMaterial(&cubeTextures);

    u32 curi = AddObject(info, info->cubeVao, 36, position, &cubeTextures);
    AddObjectToShaderPass(&info->dirDepthMapShader, curi);
    AddObjectToShaderPass(&info->spotDepthMapShader, curi);
    AddObjectToShaderPass(&info->pointDepthMapShader, curi);
    AddObjectToShaderPass(&info->gBufferShader, curi);
    AddObjectToShaderPass(&info->ssaoShader, curi);
    AddObjectToShaderPass(&info->ssaoBlurShader, curi);

    cubes->numCubes++;
}

internal void CreateQuad(TransientDrawingInfo *transientInfo, Arena *texturesArena)
{
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
    u32 mainQuadVao = CreateVAO((f32 *)quadVertices, sizeof(quadVertices), gBitangentElemCounts,
                                myArraySize(gBitangentElemCounts), quadIndices, sizeof(quadIndices));
    transientInfo->quadVao = mainQuadVao;
}

extern "C" __declspec(dllexport) bool InitializeDrawingInfo(HWND window, TransientDrawingInfo *transientInfo,
                                                            PersistentDrawingInfo *drawingInfo, CameraInfo *cameraInfo)
{
    u64 arenaSize = 100 * 1024 * 1024;

    if (!CreateShaderPrograms(transientInfo))
    {
        return false;
    }

    // TODO: sort out arena usage; don't use texturesArena for anything other than texture, or if you do
    // then rename it.
    Arena *texturesArena = AllocArena(1024);
    Arena *meshDataArena = AllocArena(100 * 1024 * 1024);
    LoadModels(transientInfo, texturesArena, meshDataArena);
    LoadCube(transientInfo, texturesArena);
    CreateQuad(transientInfo, texturesArena);
    FreeArena(meshDataArena);
    FreeArena(texturesArena);

    srand((u32)Win32GetWallClock());

    // SetUpAsteroids(&transientInfo->models[2]);

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

    drawingInfo->dirLight.direction =
        glm::normalize(pointLights[NUM_POINTLIGHTS - 1].position - pointLights[0].position);

    LoadDrawingInfo(transientInfo, drawingInfo, cameraInfo);

    CreateFramebuffers(window, transientInfo);

    CreateSkybox(transientInfo);
    glDepthFunc(GL_LEQUAL); // All skybox points are given a depth of 1.f.

    u32 *matricesUBO = &transientInfo->matricesUBO;
    glCreateBuffers(1, matricesUBO);
    char label[] = "UBO: matrices";
    glObjectLabel(GL_BUFFER, *matricesUBO, -1, label);
    glNamedBufferData(*matricesUBO, 10 * sizeof(glm::mat4), NULL, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, *matricesUBO);

    u32 *textureHandlesUBO = &transientInfo->textureHandlesUBO;
    glCreateBuffers(1, textureHandlesUBO);
    glNamedBufferData(*textureHandlesUBO, sizeof(TextureHandles) * MAX_MESHES_PER_MODEL, NULL, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, *textureHandlesUBO);

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

internal void RenderObject(Object *object, u32 shaderProgram, u32 textureHandlesUBO, f32 yRot = 0.f, float scale = 1.f)
{
    glBindVertexArray(object->vao);

    glNamedBufferSubData(textureHandlesUBO, 0, sizeof(object->textures), &object->textures);
    SetShaderUniformInt(shaderProgram, "displace", object->textures.displacementHandle > 0);

    // Model matrix: transforms vertices from local to world space.
    glm::mat4 modelMatrix = glm::mat4(1.f);
    modelMatrix = glm::translate(modelMatrix, object->position);
    modelMatrix = glm::rotate(modelMatrix, yRot, glm::vec3(0.f, 1.f, 0.f));
    modelMatrix = glm::scale(modelMatrix, glm::vec3(scale));
    glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(modelMatrix)));

    SetShaderUniformMat4(shaderProgram, "modelMatrix", &modelMatrix);
    SetShaderUniformMat3(shaderProgram, "normalMatrix", &normalMatrix);
    SetShaderUniformUint(shaderProgram, "u_objectId", object->id);
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
        // NOTE: id = 0 because we don't care about selecting outlines.
        Object lightObject = {0, transientInfo->cubeVao, 36, curLight->position};
        RenderObject(&lightObject, shaderProgram, transientInfo->textureHandlesUBO, 0.f, .1f);

        glStencilMask(0x00);
        glStencilFunc(GL_NOTEQUAL, 1, 0xff);

        glm::vec4 stencilColor = glm::vec4(0.f, 0.f, 1.f, 1.f);
        SetShaderUniformVec3(shaderProgram, "color", stencilColor);
        RenderObject(&lightObject, shaderProgram, transientInfo->textureHandlesUBO, 0.f, .11f);

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

internal void RenderModel(Model *model, u32 shaderProgram, u32 textureHandlesUBO, u32 skyboxTexture = 0,
                          u32 numInstances = 1)
{
    glUseProgram(shaderProgram);
    glBindVertexArray(model->vao);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, model->commandBuffer);
    glNamedBufferSubData(textureHandlesUBO, 0, sizeof(model->textureHandleBuffer.handleGroups),
                         model->textureHandleBuffer.handleGroups);

    // Model matrix: transforms vertices from local to world space.
    glm::mat4 modelMatrix = glm::mat4(1.f);
    modelMatrix = glm::translate(modelMatrix, model->position);
    modelMatrix = glm::scale(modelMatrix, model->scale);

    // TODO: store relative transforms and textures somewhere indexed and shader-accessible.
    // modelMatrix *= mesh->relativeTransform;

    glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(modelMatrix)));

    SetShaderUniformMat4(shaderProgram, "modelMatrix", &modelMatrix);
    SetShaderUniformMat3(shaderProgram, "normalMatrix", &normalMatrix);

    // TODO: just have a single command buffer and have each model keep an offset into it?
    glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0, model->meshCount, 0);

    // TODO: figure out instanced rendering with MDI.
    /*
    for (u32 i = 0; i < model->meshCount; i++)
    {
        if (numInstances == 1)
        {

            glDrawElements(GL_TRIANGLES, mesh->indicesSize / sizeof(u32), GL_UNSIGNED_INT, 0);
        }
        else
        {
            SetShaderUniformFloat(shaderProgram, "time", Win32GetTime());
            glDrawElementsInstanced(GL_TRIANGLES, mesh->indicesSize / sizeof(u32), GL_UNSIGNED_INT, 0, numInstances);
        }
    }
    */
}

internal void RenderShaderPass(ShaderProgram *shaderProgram, TransientDrawingInfo *transientInfo)
{
    glUseProgram(shaderProgram->id);

    for (u32 i = 0; i < shaderProgram->numObjects; i++)
    {
        u32 curIndex = shaderProgram->objectIndices[i];
        Object *curObject = &transientInfo->objects[curIndex];
        RenderObject(curObject, shaderProgram->id, transientInfo->textureHandlesUBO);
    }
    for (u32 i = 0; i < shaderProgram->numModels; i++)
    {
        u32 curIndex = shaderProgram->modelIndices[i];
        RenderModel(&transientInfo->models[curIndex], shaderProgram->id, transientInfo->textureHandlesUBO);
    }
}

internal void FlipImage(u8 *data, s32 width, s32 height, u32 bytesPerPixel, Arena *arena)
{
    u32 stride = width * bytesPerPixel;
    u8 *tmp = (u8 *)ArenaPush(arena, stride);
    u8 *end = data + width * height * bytesPerPixel;
    for (s32 i = 0; i < height / 2; i++)
    {
        memcpy_s(tmp, stride, data + i * stride, stride);
        memcpy_s(data + i * stride, stride, end - (1 + i) * stride, stride);
        memcpy_s(end - (1 + i) * stride, stride, tmp, stride);
    }
    ArenaPop(arena, stride);
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
                                        PersistentDrawingInfo *persistentInfo)
{
    u32 *mainQuads = transientInfo->mainFramebuffer.attachments;
    glBindTextureUnit(10, mainQuads[0]);
    glBindTextureUnit(11, mainQuads[1]);
    glBindTextureUnit(12, mainQuads[2]);
    glBindTextureUnit(13, transientInfo->ssaoFramebuffer.attachments[0]);
    glBindTextureUnit(14, transientInfo->skyboxTexture);
    glBindTextureUnit(15, transientInfo->dirShadowMapFramebuffer.attachments[0]);
    glBindTextureUnit(16, transientInfo->spotShadowMapFramebuffer.attachments[0]);

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
    glNamedBufferSubData(transientInfo->matricesUBO, 0, 64, &identity);
    glNamedBufferSubData(transientInfo->matricesUBO, 64, 64, &identity);

    glBindVertexArray(transientInfo->quadVao);
    glUseProgram(shaderProgram);
    SetShaderUniformMat4(shaderProgram, "modelMatrix", &identity);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

internal void ExecuteLightingPass(CameraInfo *cameraInfo, TransientDrawingInfo *transientInfo,
                                  PersistentDrawingInfo *persistentInfo, HWND window, Arena *listArena, Arena *arena,
                                  bool dynamicEnvPass = false)
{
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Lighting pass");
    TracyGpuZone("Lighting pass");

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
            glNamedBufferSubData(transientInfo->matricesUBO, 0, 64, &savedViewMatrix);
            glNamedBufferSubData(transientInfo->matricesUBO, 64, 64, &savedProjectionMatrix);
        }
        else
        {
            return;
        }
    }
#endif

    glBindFramebuffer(GL_FRAMEBUFFER, transientInfo->lightingFramebuffer.fbo);
    GLenum attachments[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, attachments);
    glClearColor(1.f, 1.f, 1.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Non-point lighting.
    u32 nonPointShaderProgram = transientInfo->nonPointLightingShader.id;
    glUseProgram(nonPointShaderProgram);
    SetLightingShaderUniforms(cameraInfo, transientInfo, persistentInfo);

    RenderQuad(transientInfo, nonPointShaderProgram);

    // Point lighting.
    // NOTE: perhaps we could do instanced rendering of the spheres instead?
    u32 pointShader = transientInfo->pointLightingShader.id;
    glUseProgram(pointShader);

    glm::mat4 viewMatrix, projectionMatrix;
    GetPerspectiveRenderingMatrices(cameraInfo, &viewMatrix, &projectionMatrix);
    glNamedBufferSubData(transientInfo->matricesUBO, 0, 64, &viewMatrix);
    glNamedBufferSubData(transientInfo->matricesUBO, 64, 64, &projectionMatrix);

    RECT clientRect;
    GetClientRect(window, &clientRect);
    s32 width = clientRect.right;
    s32 height = clientRect.bottom;

    glm::vec2 screenSize{(f32)width, (f32)height};
    SetShaderUniformVec2(pointShader, "screenSize", screenSize);

    glEnable(GL_STENCIL_TEST);
    u32 blitSource = transientInfo->mainFramebuffer.fbo;
    u32 blitDest = transientInfo->lightingFramebuffer.fbo;
    glBlitNamedFramebuffer(blitSource, blitDest, 0, 0, width, height, 0, 0, width, height, GL_DEPTH_BUFFER_BIT,
                           GL_NEAREST);
    glNamedFramebufferDrawBuffers(blitDest, 2, attachments);

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

        glBindVertexArray(model->vao);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, model->commandBuffer);

        glm::mat4 lightModelMatrix = glm::mat4(1.f);
        lightModelMatrix = glm::translate(lightModelMatrix, light.position);
        f32 constant = 1.f;
        f32 linear = att->linear;
        f32 quadratic = att->quadratic;
        f32 lightMax = fmax(fmax(light.diffuse.r, light.diffuse.g), light.diffuse.b);
        f32 radius = (-linear + sqrtf(linear * linear - 4.f * quadratic * (constant - (256.f / 5.f) * lightMax))) /
                     (2.f * quadratic);
        lightModelMatrix = glm::scale(lightModelMatrix, glm::vec3(radius));

        // TODO: restore use of relativeTransform.
        // lightModelMatrix *= mesh->relativeTransform;

        glm::mat3 lightNormalMatrix = glm::mat3(glm::transpose(glm::inverse(lightModelMatrix)));

        SetShaderUniformMat4(pointShader, "modelMatrix", &lightModelMatrix);
        SetShaderUniformMat3(pointShader, "normalMatrix", &lightNormalMatrix);

        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0, model->meshCount, 0);

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

        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0, model->meshCount, 0);

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
    // for (u32 i = 0; i < NUM_CUBES; i++)
    // {
    //     f32 dist = glm::distance(cameraInfo->pos, persistentInfo->windowPos[i]);
    //     Insert(&list, dist, persistentInfo->windowPos[i], listArena, tempArena);
    // }

    // for (u32 i = 0; i < NUM_CUBES; i++)
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
    TracyGpuZone("DrawScene");

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

    glNamedBufferSubData(transientInfo->matricesUBO, 0, 64, &viewMatrix);
    glNamedBufferSubData(transientInfo->matricesUBO, 64, 64, &projectionMatrix);
    glNamedBufferSubData(transientInfo->matricesUBO, 128, 64, &dirLightSpaceMatrix);
    glNamedBufferSubData(transientInfo->matricesUBO, 192, 64, &spotLightSpaceMatrix);

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

void DrawEditorMenu(ApplicationState *appState, CameraInfo *cameraInfo)
{
    TransientDrawingInfo *transientInfo = &appState->transientInfo;
    PersistentDrawingInfo *persistentInfo = &appState->persistentInfo;

    u32 id = 0;
    ImGui::Begin("Editor Menu");

    if (ImGui::Button("Toggle playing"))
    {
        bool *playing = &appState->playing;
        *playing = !*playing;
        if (*playing)
        {
            // TODO: fix this disgusting hard-coding. Model-related functions should really be
            // operating with Model pointers, not indices. See also:
            // - note in Model struct;
            // - note + todo above AddModelToShaderPass().
            transientInfo->ball.model = transientInfo->sphereModel;
            transientInfo->models[4].position = transientInfo->ball.position;
            AddModelToShaderPass(&transientInfo->dirDepthMapShader, 4);
            AddModelToShaderPass(&transientInfo->spotDepthMapShader, 4);
            AddModelToShaderPass(&transientInfo->pointDepthMapShader, 4);
            AddModelToShaderPass(&transientInfo->gBufferShader, 4);
            AddModelToShaderPass(&transientInfo->geometryShader, 4);
            AddModelToShaderPass(&transientInfo->ssaoShader, 4);
            AddModelToShaderPass(&transientInfo->ssaoBlurShader, 4);
        }
    }

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

    if (ImGui::CollapsingHeader("Cubes"))
    {
        local_persist glm::ivec3 position;
        ImGui::SliderInt3("Position", glm::value_ptr(position), -10, 10);
        if (ImGui::Button("Add cube"))
        {
            AddCube(transientInfo, position);
        }
    }

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
    //     for (u32 index = 0; index < NUM_CUBES; index++)
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

internal void DrawSkybox(TransientDrawingInfo *transientInfo, CameraInfo *cameraInfo, s32 width, s32 height)
{
    // Draw skybox where geometry rendering pass did not set stencil value to 1.
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Skybox pass");
    TracyGpuZone("Skybox pass");

    u32 blitSource = transientInfo->mainFramebuffer.fbo;
    u32 blitDest = transientInfo->lightingFramebuffer.fbo;
    glBlitNamedFramebuffer(blitSource, blitDest, 0, 0, width, height, 0, 0, width, height, GL_STENCIL_BUFFER_BIT,
                           GL_NEAREST);

    glEnable(GL_STENCIL_TEST);
    glStencilMask(0x00);
    glStencilFunc(GL_NOTEQUAL, 1, 0xff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    u32 shaderProgram = transientInfo->skyboxShader.id;
    glUseProgram(shaderProgram);

    glm::mat4 viewMatrix, projectionMatrix;
    GetPerspectiveRenderingMatrices(cameraInfo, &viewMatrix, &projectionMatrix);
    glm::mat4 skyboxViewMatrix = glm::mat4(glm::mat3(viewMatrix));
    glNamedBufferSubData(transientInfo->matricesUBO, 0, 64, &skyboxViewMatrix);
    glNamedBufferSubData(transientInfo->matricesUBO, 64, 64, &projectionMatrix);

    glBindVertexArray(transientInfo->cubeVao);
    glBindTextureUnit(10, transientInfo->skyboxTexture);

    glBindFramebuffer(GL_FRAMEBUFFER, blitDest);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

    glDisable(GL_STENCIL_TEST);

    glPopDebugGroup();
}

internal void ExecuteSSAOPass(TransientDrawingInfo *transientInfo, PersistentDrawingInfo *persistentInfo,
                              CameraInfo *cameraInfo, glm::vec2 screenSize)
{
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "SSAO pass");
    TracyGpuZone("SSAO pass");

    {
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Sampling subpass");
        TracyGpuZone("Sampling subpass");

        glBindFramebuffer(GL_FRAMEBUFFER, transientInfo->ssaoFramebuffer.fbo);
        glClear(GL_COLOR_BUFFER_BIT);

        u32 shaderProgram = transientInfo->ssaoShader.id;
        glUseProgram(shaderProgram);
        glBindTextureUnit(10, transientInfo->mainFramebuffer.attachments[0]);
        glBindTextureUnit(11, transientInfo->mainFramebuffer.attachments[1]);
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
    }

    {
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Noise subpass");
        TracyGpuZone("Noise subpass");

        glBindFramebuffer(GL_FRAMEBUFFER, transientInfo->ssaoBlurFramebuffer.fbo);
        glClear(GL_COLOR_BUFFER_BIT);

        u32 shaderProgram = transientInfo->ssaoBlurShader.id;
        glUseProgram(shaderProgram);
        glBindTextureUnit(10, transientInfo->ssaoFramebuffer.attachments[0]);
        RenderQuad(transientInfo, shaderProgram);

        glPopDebugGroup();
    }

    glPopDebugGroup();
}

extern "C" __declspec(dllexport) void DrawWindow(HWND window, HDC hdc, ApplicationState *appState, Arena *listArena,
                                                 Arena *tempArena)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    TransientDrawingInfo *transientInfo = &appState->transientInfo;
    PersistentDrawingInfo *persistentInfo = &appState->persistentInfo;
    CameraInfo *inCameraInfo = &appState->cameraInfo;

    if (!persistentInfo->initialized)
    {
        return;
    }

    CheckForNewShaders(transientInfo);

    RECT clientRect;
    GetClientRect(window, &clientRect);
    s32 width = clientRect.right;
    s32 height = clientRect.bottom;

    CameraInfo playingCameraInfo = *inCameraInfo;
    glm::ivec3 ballRotation = transientInfo->ball.rotation;
    playingCameraInfo.pos = transientInfo->ball.position - ballRotation * 3;
    playingCameraInfo.yaw = 0;
    playingCameraInfo.pitch = 0;
    playingCameraInfo.forwardVector = GetCameraForwardVector(&playingCameraInfo);
    playingCameraInfo.rightVector = GetCameraRightVector(&playingCameraInfo);

    CameraInfo *cameraInfo = appState->playing ? &playingCameraInfo : inCameraInfo;

    // Directional shadow map pass.
    // NOTE: front-face culling is a sledgehammer solution to Peter-Panning and may break down with
    // some objects.
    glViewport(0, 0, DIR_SHADOW_MAP_SIZE, DIR_SHADOW_MAP_SIZE);
    glCullFace(GL_FRONT);
    DrawScene(cameraInfo, transientInfo, persistentInfo, transientInfo->dirShadowMapFramebuffer.fbo, window, listArena,
              tempArena, false, RenderPassType::DirShadowMap);

    // Spot shadow map pass.
    DrawScene(cameraInfo, transientInfo, persistentInfo, transientInfo->spotShadowMapFramebuffer.fbo, window, listArena,
              tempArena, false, RenderPassType::SpotShadowMap);
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

        for (u32 j = 0; j < 6; j++)
        {
            glNamedBufferSubData(transientInfo->matricesUBO, 256 + j * 64, 64, &pointShadowMatrices[j]);
        }
        u32 pointShaderProgram = transientInfo->pointDepthMapShader.id;
        glUseProgram(pointShaderProgram);
        SetShaderUniformVec3(pointShaderProgram, "lightPos", pointCameraInfo.pos);
        SetShaderUniformFloat(pointShaderProgram, "farPlane", pointFar);
        u32 lightingShaderProgram = transientInfo->pointLightingShader.id;
        glUseProgram(lightingShaderProgram);
        SetShaderUniformFloat(lightingShaderProgram, "pointFar", pointFar);

        DrawScene(&pointCameraInfo, transientInfo, persistentInfo, transientInfo->pointShadowMapFBO[i], window,
                  listArena, tempArena, false, RenderPassType::PointShadowMap);
    }

    glViewport(0, 0, width, height);
    // Main pass.
    DrawScene(cameraInfo, transientInfo, persistentInfo, transientInfo->mainFramebuffer.fbo, window, listArena,
              tempArena);

    glDisable(GL_DEPTH_TEST);

    // Main SSAO pass.
    glm::vec2 screenSize(width, height);
    ExecuteSSAOPass(transientInfo, persistentInfo, cameraInfo, screenSize);

    // Main lighting pass.
    ExecuteLightingPass(cameraInfo, transientInfo, persistentInfo, window, listArena, tempArena);

    // Main skybox pass.
    DrawSkybox(transientInfo, cameraInfo, width, height);

    // Apply Gaussian blur to brightness texture to generate bloom.
    {
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Gaussian blur for bloom");
        TracyGpuZone("Gaussian blur for bloom");
        bool horizontal = true;
        u32 gaussianShader = transientInfo->gaussianShader.id;
        u32 gaussianQuad = transientInfo->lightingFramebuffer.attachments[1];
        glUseProgram(gaussianShader);
        for (u32 i = 0; i < 10; i++)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, transientInfo->gaussianFramebuffers[horizontal].fbo);
            SetShaderUniformInt(gaussianShader, "horizontal", horizontal);
            glBindTextureUnit(10, gaussianQuad);
            horizontal = !horizontal;

            RenderQuad(transientInfo, gaussianShader);

            gaussianQuad = transientInfo->gaussianFramebuffers[!horizontal].attachments[0];
        }
        glPopDebugGroup();
    }

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
        TracyGpuZone("Main quad post-processing");

        glBindTextureUnit(10, transientInfo->lightingFramebuffer.attachments[0]);
        glBindTextureUnit(11, transientInfo->gaussianFramebuffers[0].attachments[0]);

        RenderQuad(transientInfo, shaderProgram);

        glPopDebugGroup();
    }

    glEnable(GL_DEPTH_TEST);

    // Point lights.
    // TODO: fix effect of outlining on meshes that appear between the camera and the outlined
    // object.
    {
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Point lights");
        TracyGpuZone("Point lights");

        glBlitNamedFramebuffer(transientInfo->mainFramebuffer.fbo, 0, 0, 0, width, height, 0, 0, width, height,
                               GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        glm::mat4 viewMatrix, projectionMatrix;
        GetPerspectiveRenderingMatrices(cameraInfo, &viewMatrix, &projectionMatrix);
        glNamedBufferSubData(transientInfo->matricesUBO, 0, 64, &viewMatrix);
        glNamedBufferSubData(transientInfo->matricesUBO, 64, 64, &projectionMatrix);
        // RenderWithColorShader(transientInfo, persistentInfo);
        glPopDebugGroup();
    }

    DrawEditorMenu(appState, cameraInfo);

    if (!SwapBuffers(hdc))
    {
        if (MessageBoxW(window, L"Failed to swap buffers", L"OpenGL error", MB_OK) == S_OK)
        {
            appState->running = false;
        }
    }

    TracyGpuCollect;
}

#include "arena.h"
#include "common.h"
#include "render.h"
#include "texture.h"

internal Mesh ProcessMesh(aiMesh *mesh, const aiScene *scene, Arena *texturesArena, LoadedTextures *loadedTextures,
                          Arena *vertices, Arena *indices, DrawElementsIndirectCommand *command,
                          TextureHandles *handles)
{
    Mesh result = {};

    command->baseVertex = (u32)(vertices->stackPointer / sizeof(Vertex));
    result.verticesSize = mesh->mNumVertices * sizeof(Vertex);
    result.vertices = (Vertex *)ArenaPush(vertices, result.verticesSize);
    myAssert(((u8 *)result.vertices + result.verticesSize) == ((u8 *)vertices->memory + vertices->stackPointer));

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

    command->firstIndex = (u32)(indices->stackPointer / sizeof(u32));
    result.indices = (u32 *)((u8 *)indices->memory + indices->stackPointer);
    u32 indicesCount = 0;
    for (u32 i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];

        u32 *faceIndices = (u32 *)ArenaPush(indices, sizeof(u32) * face.mNumIndices);
        for (u32 j = 0; j < face.mNumIndices; j++)
        {
            faceIndices[j] = face.mIndices[j];
            indicesCount++;
        }
    }
    command->count = indicesCount;
    result.indicesSize = indicesCount * sizeof(u32);
    myAssert(((u8 *)result.indices + result.indicesSize) == ((u8 *)indices->memory + indices->stackPointer));

    if (mesh->mMaterialIndex >= 0)
    {
        aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];

        u32 numDiffuse = material->GetTextureCount(aiTextureType_DIFFUSE);
        u32 numSpecular = material->GetTextureCount(aiTextureType_SPECULAR);
        u32 numNormals = material->GetTextureCount(aiTextureType_HEIGHT);
        u32 numDisp = material->GetTextureCount(aiTextureType_DISPLACEMENT);
        u32 numTextures = numDiffuse + numSpecular + numNormals + numDisp;

        u64 texturesSize = sizeof(Texture) * numTextures;

        Material *textures = &result.material;
        LoadTextures(&result, &textures->diffuse, numDiffuse, material, aiTextureType_DIFFUSE, texturesArena,
                     loadedTextures);
        LoadTextures(&result, &textures->specular, numSpecular, material, aiTextureType_SPECULAR, texturesArena,
                     loadedTextures);
        LoadTextures(&result, &textures->normals, numNormals, material, aiTextureType_HEIGHT, texturesArena,
                     loadedTextures);
        LoadTextures(&result, &textures->displacement, numDisp, material, aiTextureType_DISPLACEMENT, texturesArena,
                     loadedTextures);

        *handles = CreateTextureHandlesFromMaterial(textures);
    }

    return result;
}

internal void ProcessNode(aiNode *node, const aiScene *scene, Mesh *meshes, u32 *meshCount, Arena *texturesArena,
                          LoadedTextures *loadedTextures, Arena *vertices, Arena *indices,
                          IndirectCommandBuffer *commandBuffer, TextureHandleBuffer *texHandleBuffer)
{
    for (u32 i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];

        DrawElementsIndirectCommand *command = &commandBuffer->commands[commandBuffer->numCommands];
        TextureHandles *handles = &texHandleBuffer->handleGroups[texHandleBuffer->numHandleGroups];
        Mesh processedMesh =
            ProcessMesh(mesh, scene, texturesArena, loadedTextures, vertices, indices, command, handles);
        commandBuffer->numCommands++;
        texHandleBuffer->numHandleGroups++;

        myAssert(commandBuffer->numCommands <= MAX_MESHES_PER_MODEL);
        myAssert(texHandleBuffer->numHandleGroups <= MAX_MESHES_PER_MODEL);
        myAssert(commandBuffer->numCommands == texHandleBuffer->numHandleGroups);

        aiMatrix4x4 trans = node->mTransformation;
        glm::mat4 rowMajorTrans = {trans.a1, trans.a2, trans.a3, trans.a4, trans.b1, trans.b2, trans.b3, trans.b4,
                                   trans.c1, trans.c2, trans.c3, trans.c4, trans.d1, trans.d2, trans.d3, trans.d4};
        processedMesh.relativeTransform = glm::transpose(rowMajorTrans);
        meshes[*meshCount] = processedMesh;
        *meshCount += 1;
    }

    for (u32 i = 0; i < node->mNumChildren; i++)
    {
        ProcessNode(node->mChildren[i], scene, meshes, meshCount, texturesArena, loadedTextures, vertices, indices,
                    commandBuffer, texHandleBuffer);
    }
}

internal Model LoadModel(const char *filename, s32 *elemCounts, u32 elemCountsSize, Arena *texturesArena,
                         Arena *meshDataArena, u32 *objectId, f32 scale = 1.f)
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

    Arena *vertices = AllocArena(1000000 * sizeof(Vertex));
    Arena *indices = AllocArena(1500000 * sizeof(u32));
    IndirectCommandBuffer commandBuffer = {};
    TextureHandleBuffer *texHandleBuffer = &result.textureHandleBuffer;
    ProcessNode(scene->mRootNode, scene, meshes, &meshCount, texturesArena, &loadedTextures, vertices, indices,
                &commandBuffer, texHandleBuffer);
    myAssert(commandBuffer.numCommands == meshCount);
    myAssert(texHandleBuffer->numHandleGroups == meshCount);

    u32 *vao = &result.vao;
    glCreateVertexArrays(1, vao);

    u32 vbo;
    glCreateBuffers(1, &vbo);
    glNamedBufferStorage(vbo, vertices->stackPointer, vertices->memory, 0);

    u32 ebo;
    glCreateBuffers(1, &ebo);
    glNamedBufferStorage(ebo, indices->stackPointer, indices->memory, 0);

    glVertexArrayVertexBuffer(*vao, 0, vbo, 0, sizeof(Vertex));
    glVertexArrayElementBuffer(*vao, ebo);

    glVertexArrayAttribFormat(*vao, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribFormat(*vao, 1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(f32));
    glVertexArrayAttribFormat(*vao, 2, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(f32));
    glVertexArrayAttribFormat(*vao, 3, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(f32));
    glVertexArrayAttribFormat(*vao, 4, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(f32));
    glVertexArrayAttribBinding(*vao, 0, 0);
    glVertexArrayAttribBinding(*vao, 1, 0);
    glVertexArrayAttribBinding(*vao, 2, 0);
    glVertexArrayAttribBinding(*vao, 3, 0);
    glVertexArrayAttribBinding(*vao, 4, 0);
    glEnableVertexArrayAttrib(*vao, 0);
    glEnableVertexArrayAttrib(*vao, 1);
    glEnableVertexArrayAttrib(*vao, 2);
    glEnableVertexArrayAttrib(*vao, 3);
    glEnableVertexArrayAttrib(*vao, 4);

    u32 *icb = &result.commandBuffer;
    glCreateBuffers(1, icb);
    glNamedBufferStorage(*icb, sizeof(DrawElementsIndirectCommand) * commandBuffer.numCommands, commandBuffer.commands,
                         0);

    FreeArena(vertices);
    FreeArena(indices);

    result.meshCount = meshCount;
    result.scale = glm::vec3(scale);

    result.id = *objectId++;

    return result;
}

internal u32 AddModel(const char *filename, TransientDrawingInfo *transientInfo, s32 *elemCounts, u32 elemCountsSize,
                      Arena *texturesArena, Arena *meshDataArena, u32 *objectId, f32 scale = 1.f)
{
    u32 modelIndex = transientInfo->numModels;
    transientInfo->models[modelIndex] =
        LoadModel(filename, elemCounts, elemCountsSize, texturesArena, meshDataArena, objectId, scale);
    transientInfo->numModels++;
    myAssert(transientInfo->numModels <= MAX_MODELS);
    return modelIndex;
}

internal u32 AddObject(TransientDrawingInfo *transientInfo, u32 vao, u32 numIndices, glm::vec3 position,
                       u32 *objectId, Material *textures = nullptr)
{
    u32 objectIndex = transientInfo->numObjects;
    transientInfo->objects[objectIndex] = {*objectId++, vao, numIndices, position};
    if (textures)
    {
        transientInfo->objects[objectIndex].textures = CreateTextureHandlesFromMaterial(textures);
    }
    transientInfo->numObjects++;
    myAssert(transientInfo->numObjects <= MAX_OBJECTS);
    return objectIndex;
}

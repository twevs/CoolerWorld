#include "arena.h"
#include "common.h"

internal u32 CreateVAO(VaoInformation *vaoInfo)
{
    u32 vao;
    glCreateVertexArrays(1, &vao);

    u32 vbo;
    glCreateBuffers(1, &vbo);
    glNamedBufferData(vbo, vaoInfo->verticesSize, vaoInfo->vertices, GL_STATIC_DRAW);

    u32 ebo;
    glCreateBuffers(1, &ebo);
    glNamedBufferData(ebo, vaoInfo->indicesSize, vaoInfo->indices, GL_STATIC_DRAW);

    u32 total = 0;
    for (u32 elemCount = 0; elemCount < vaoInfo->elementCountsSize; elemCount++)
    {
        total += vaoInfo->elemCounts[elemCount];
    }

    glVertexArrayVertexBuffer(vao, 0, vbo, 0, total * sizeof(float));
    glVertexArrayElementBuffer(vao, ebo);

    u32 accumulator = 0;
    for (u32 index = 0; index < vaoInfo->elementCountsSize; index++)
    {
        u32 elemCount = vaoInfo->elemCounts[index];

        glVertexArrayAttribFormat(vao, index, elemCount, GL_FLOAT, GL_FALSE, accumulator * sizeof(float));
        glVertexArrayAttribBinding(vao, index, 0);
        glEnableVertexArrayAttrib(vao, index);

        accumulator += elemCount;
    }

    return vao;
}

// Returns the size of the previous buffer.
internal u32 AppendToVAO(u32 vao, void *data, u32 dataSize)
{
    glBindVertexArray(vao);
    s32 bufferSize;
    glGetNamedBufferParameteriv(vao, GL_BUFFER_SIZE, &bufferSize);
    Arena *tempArena = AllocArena(bufferSize);
    void *savedBuffer = ArenaPush(tempArena, bufferSize);
    glGetNamedBufferSubData(vao, 0, bufferSize, savedBuffer);
    glNamedBufferData(vao, bufferSize + dataSize, NULL, GL_STATIC_DRAW);
    glNamedBufferSubData(vao, 0, bufferSize, savedBuffer);
    glNamedBufferSubData(vao, bufferSize, dataSize, data);
    FreeArena(tempArena);
    return bufferSize;
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

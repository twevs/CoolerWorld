#include "asteroids.h"
#include "arena.h"
#include "common.h"

internal void SetUpAsteroids(Model *asteroid)
{
    u32 modelMatricesSize = NUM_ASTEROIDS * sizeof(glm::mat4);
    u32 radiiSize = NUM_ASTEROIDS * sizeof(f32);
    u32 yValuesSize = NUM_ASTEROIDS * sizeof(f32);

    Arena *arena = AllocArena(modelMatricesSize + radiiSize + yValuesSize);
    glm::mat4 *modelMatrices = (glm::mat4 *)ArenaPush(arena, modelMatricesSize);
    f32 *radii = (f32 *)ArenaPush(arena, radiiSize);
    f32 *yValues = (f32 *)ArenaPush(arena, yValuesSize);
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
    u32 matricesVBO[3];
    glCreateBuffers(3, matricesVBO);
    glNamedBufferData(matricesVBO[0], modelMatricesSize, modelMatrices, GL_STATIC_DRAW);
    glNamedBufferData(matricesVBO[1], radiiSize, radii, GL_STATIC_DRAW);
    glNamedBufferData(matricesVBO[2], yValuesSize, yValues, GL_STATIC_DRAW);

    /*
    for (u32 i = 0; i < asteroid->meshCount; i++)
    {
        u32 curVao = asteroid->vaos[i];

        // NOTE: binding index 0 is already taken by the main VBO.
        glVertexArrayVertexBuffer(curVao, 1, matricesVBO[0], 0, 16 * sizeof(f32));
        glVertexArrayVertexBuffer(curVao, 2, matricesVBO[1], 0, sizeof(f32));
        glVertexArrayVertexBuffer(curVao, 3, matricesVBO[2], 0, sizeof(f32));

        glVertexArrayAttribBinding(curVao, 5, 1);
        glVertexArrayAttribBinding(curVao, 6, 1);
        glVertexArrayAttribBinding(curVao, 7, 1);
        glVertexArrayAttribBinding(curVao, 8, 1);
        glVertexArrayAttribBinding(curVao, 9, 2);
        glVertexArrayAttribBinding(curVao, 10, 3);

        glVertexArrayBindingDivisor(curVao, 1, 1);
        glVertexArrayBindingDivisor(curVao, 2, 1);
        glVertexArrayBindingDivisor(curVao, 3, 1);

        glVertexArrayAttribFormat(curVao, 5, 4, GL_FLOAT, GL_FALSE, (u64)0);
        glVertexArrayAttribFormat(curVao, 6, 4, GL_FLOAT, GL_FALSE, 0 + 4 * sizeof(f32));
        glVertexArrayAttribFormat(curVao, 7, 4, GL_FLOAT, GL_FALSE, 0 + 8 * sizeof(f32));
        glVertexArrayAttribFormat(curVao, 8, 4, GL_FLOAT, GL_FALSE, 0 + 12 * sizeof(f32));
        glVertexArrayAttribFormat(curVao, 9, 1, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribFormat(curVao, 10, 1, GL_FLOAT, GL_FALSE, 0);

        glEnableVertexArrayAttrib(curVao, 5);
        glEnableVertexArrayAttrib(curVao, 6);
        glEnableVertexArrayAttrib(curVao, 7);
        glEnableVertexArrayAttrib(curVao, 8);
        glEnableVertexArrayAttrib(curVao, 9);
        glEnableVertexArrayAttrib(curVao, 10);
    }
    */
    FreeArena(arena);
}
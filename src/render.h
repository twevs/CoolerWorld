#pragma once

#include "common.h"

struct DrawElementsIndirectCommand
{
    u32 count;
    u32 instanceCount = 1;
    u32 firstIndex = 0;
    s32 baseVertex;
    u32 baseInstance;
};

struct IndirectCommandBuffer
{
    DrawElementsIndirectCommand commands[MAX_MESHES_PER_MODEL];
    u32 numCommands = 0;
};
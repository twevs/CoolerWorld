#pragma once

#include "common.h"

struct LoadedTextures
{
    Texture *textures;
    u32 numTextures;
};

internal void LoadTextures(Mesh *mesh, Texture *texture, u64 num, aiMaterial *material, aiTextureType type,
                           struct Arena *arena, LoadedTextures *loadedTextures);
internal TextureHandles CreateTextureHandlesFromMaterial(Material *material);

#pragma once

#include "common.h"

struct FramebufferOptions
{
    GLenum internalFormat = GL_RGB8;
    GLenum pixelFormat = GL_RGB;
    GLenum type = GL_UNSIGNED_BYTE;
    GLenum filteringMethod = GL_LINEAR;
    GLenum wrapMode = GL_REPEAT;
    f32 borderColor[4] = {1.f, 1.f, 1.f, 1.f};
};

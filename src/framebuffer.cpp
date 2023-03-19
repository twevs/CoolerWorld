#include "framebuffer.h"

internal bool IsDepthbufferFormat(GLenum format)
{
    return format == GL_DEPTH_COMPONENT32F || format == GL_DEPTH_COMPONENT24 || format == GL_DEPTH_COMPONENT16;
}

internal Framebuffer CreateFramebuffer(const char *label, s32 width, s32 height, u32 numTextures,
                                       FramebufferOptions *options)
{
    Framebuffer result;

    myAssert(numTextures <= MAX_ATTACHMENTS);

    u32 *fbo = &result.fbo;
    glCreateFramebuffers(1, fbo);
    char framebufferLabel[64];
    sprintf_s(framebufferLabel, "Framebuffer: %s", label);
    glObjectLabel(GL_FRAMEBUFFER, *fbo, -1, framebufferLabel);

    u32 *quadTextures = result.attachments;
    GLenum attachments[MAX_ATTACHMENTS] = {};
    for (u32 i = 0; i < numTextures; i++)
    {
        u32 *currentTexture = &quadTextures[i];
        glCreateTextures(GL_TEXTURE_2D, 1, currentTexture);
        char textureLabel[64];
        sprintf_s(textureLabel, "Framebuffer: %s - Attached texture %i", label, i);
        glObjectLabel(GL_TEXTURE, *currentTexture, -1, textureLabel);

        GLenum internalFormat = options[i].internalFormat;
        bool depthMap = IsDepthbufferFormat(internalFormat);
        bool hdr = (internalFormat == GL_RGBA16F);
        myAssert(!(depthMap && hdr));
        myAssert(!(depthMap && (numTextures > 1)));

        myAssert(!((depthMap || hdr) && (options[i].type != GL_FLOAT)));
        glTextureStorage2D(*currentTexture, 1, internalFormat, width, height);
        glTextureSubImage2D(*currentTexture, 0, 0, 0, width, height, options[i].pixelFormat, options[i].type, NULL);
        GLint filteringMethod = options[i].filteringMethod;
        glTextureParameteri(*currentTexture, GL_TEXTURE_MIN_FILTER, filteringMethod);
        glTextureParameteri(*currentTexture, GL_TEXTURE_MAG_FILTER, filteringMethod);
        GLenum wrapMode = options[i].wrapMode;
        if (wrapMode != GL_REPEAT)
        {
            glTextureParameteri(*currentTexture, GL_TEXTURE_WRAP_S, wrapMode);
            glTextureParameteri(*currentTexture, GL_TEXTURE_WRAP_T, wrapMode);

            if (wrapMode == GL_CLAMP_TO_BORDER)
            {
                glTextureParameterfv(*currentTexture, GL_TEXTURE_BORDER_COLOR, options[i].borderColor);
            }
        }
        // TODO: non-depth map texture wrapping parameters?
        glBindTexture(GL_TEXTURE_2D, 0);

        GLenum attachment = depthMap ? GL_DEPTH_ATTACHMENT : (GL_COLOR_ATTACHMENT0 + i);
        glNamedFramebufferTexture(*fbo, attachment, *currentTexture, 0);

        attachments[i] = attachment;
    }

    if (IsDepthbufferFormat(options[0].internalFormat))
    {
        glNamedFramebufferDrawBuffer(*fbo, GL_NONE);
        glNamedFramebufferReadBuffer(*fbo, GL_NONE);
    }
    else
    {
        u32 *rbo = &result.rbo;
        glNamedFramebufferDrawBuffers(*fbo, numTextures, attachments);
        glGenRenderbuffers(1, rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, *rbo);
        char renderbufferLabel[64];
        sprintf_s(renderbufferLabel, "Renderbuffer: %s", label);
        glObjectLabel(GL_RENDERBUFFER, *rbo, -1, renderbufferLabel);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glNamedFramebufferRenderbuffer(*fbo, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, *rbo);
    }

    myAssert(glCheckNamedFramebufferStatus(*fbo, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    return result;
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

    glCreateFramebuffers(1, depthCubemapFBO);
    char framebufferLabel[64];
    sprintf_s(framebufferLabel, "Framebuffer (cube map): %s", label);
    glObjectLabel(GL_FRAMEBUFFER, *depthCubemapFBO, -1, framebufferLabel);
    glNamedFramebufferTexture(*depthCubemapFBO, GL_DEPTH_ATTACHMENT, *depthCubemap, 0);
    glNamedFramebufferDrawBuffer(*depthCubemapFBO, GL_NONE);
    glNamedFramebufferReadBuffer(*depthCubemapFBO, GL_NONE);

    return glCheckNamedFramebufferStatus(*depthCubemapFBO, GL_FRAMEBUFFER);
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
    hdrBuffer.pixelFormat = GL_RGBA;
    hdrBuffer.type = GL_FLOAT;

    FramebufferOptions pickingBuffer = {};
    pickingBuffer.internalFormat = GL_RG8UI;
    pickingBuffer.pixelFormat = GL_RG_INTEGER;

    // Main G-buffer, with 4 colour buffers:
    // 0 = position buffer, 1 = normal buffer, 2 = albedo buffer, 3 = picking buffer.
    FramebufferOptions gBufferOptions[4] = {hdrBuffer, hdrBuffer, hdrBuffer, pickingBuffer};
    auto *positionBuffer = &gBufferOptions[0];
    positionBuffer->filteringMethod = GL_NEAREST;
    positionBuffer->wrapMode = GL_CLAMP_TO_EDGE;

    transientInfo->mainFramebuffer =
        CreateFramebuffer("Main G-buffer", width, height, myArraySize(gBufferOptions), gBufferOptions);

    FramebufferOptions ssaoFramebufferOptions = {};
    ssaoFramebufferOptions.internalFormat = GL_R8;
    ssaoFramebufferOptions.pixelFormat = GL_RED;
    ssaoFramebufferOptions.type = GL_FLOAT;
    ssaoFramebufferOptions.filteringMethod = GL_NEAREST;

    transientInfo->ssaoFramebuffer = CreateFramebuffer("SSAO", width, height, 1, &ssaoFramebufferOptions);
    transientInfo->ssaoBlurFramebuffer = CreateFramebuffer("SSAO blur", width, height, 1, &ssaoFramebufferOptions);

    // 0 = HDR colour buffer, 1 = bloom threshold buffer.
    FramebufferOptions lightingFramebufferOptions[2] = {hdrBuffer, hdrBuffer};

    transientInfo->lightingFramebuffer = CreateFramebuffer(
        "Main lighting pass", width, height, myArraySize(lightingFramebufferOptions), lightingFramebufferOptions);

    transientInfo->postProcessingFramebuffer = CreateFramebuffer("Post-processing", width, height, 1, &hdrBuffer);

    FramebufferOptions depthBufferOptions = {};
    depthBufferOptions.internalFormat = GL_DEPTH_COMPONENT24;
    depthBufferOptions.pixelFormat = GL_DEPTH_COMPONENT;
    depthBufferOptions.type = GL_FLOAT;
    depthBufferOptions.filteringMethod = GL_NEAREST;
    depthBufferOptions.wrapMode = GL_CLAMP_TO_BORDER;

    transientInfo->dirShadowMapFramebuffer = CreateFramebuffer("Directional light shadow map", DIR_SHADOW_MAP_SIZE,
                                                               DIR_SHADOW_MAP_SIZE, 1, &depthBufferOptions);

    transientInfo->spotShadowMapFramebuffer =
        CreateFramebuffer("Spot light shadow map", width, height, 1, &depthBufferOptions);

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
        transientInfo->gaussianFramebuffers[i] = CreateFramebuffer(label, width, height, 1, &hdrBuffer);
    }
}
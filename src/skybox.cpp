#include "common.h"

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
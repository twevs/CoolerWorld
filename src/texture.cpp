#include "texture.h"
#include "arena.h"
#include "common.h"

internal u32 CreateTextureFromImage(const char *filename, bool sRGB, GLenum wrapMode = GL_REPEAT)
{
    s32 width;
    s32 height;
    s32 numChannels;
    stbi_set_flip_vertically_on_load_thread(true);
    uchar *textureData = stbi_load(filename, &width, &height, &numChannels, 0);
    myAssert(textureData);

    u32 texture;
    glCreateTextures(GL_TEXTURE_2D, 1, &texture);
    char label[64];
    sprintf_s(label, "Texture: %s", filename);
    glObjectLabel(GL_TEXTURE, texture, -1, label);

    glTextureParameteri(texture, GL_TEXTURE_WRAP_S, wrapMode);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_T, wrapMode);
    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    bool alpha = (numChannels == 4);
    GLenum sizedFormat = alpha ? GL_RGBA8 : (numChannels == 3) ? GL_RGB8 : GL_R8;
    GLenum pixelFormat = alpha ? GL_RGBA : (numChannels == 3) ? GL_RGB : GL_RED;
    if (sRGB)
    {
        GLenum internalFormat = alpha ? GL_SRGB8_ALPHA8 : GL_SRGB8;
        glTextureStorage2D(texture, 1, internalFormat, width, height);
        glTextureSubImage2D(texture, 0, 0, 0, width, height, pixelFormat, GL_UNSIGNED_BYTE, textureData);
    }
    else
    {
        glPixelStorei(GL_UNPACK_ALIGNMENT, alpha ? 4 : 1);
        glTextureStorage2D(texture, 1, sizedFormat, width, height);
        glTextureSubImage2D(texture, 0, 0, 0, width, height, pixelFormat, GL_UNSIGNED_BYTE, textureData);
    }
    glGenerateTextureMipmap(texture);

    stbi_image_free(textureData);

    return texture;
}

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

internal void LoadTextures(Mesh *mesh, Texture *texture, u64 num, aiMaterial *material, aiTextureType type,
                           Arena *arena, LoadedTextures *loadedTextures)
{
    myAssert(num <= 1); // NOTE: for now we only handle one texture per texture type.
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
                texture->id = loadedTextures->textures[j].id;
                texture->type = loadedTextures->textures[j].type;
                texture->hash = hash;
                skip = true;
                break;
            }
        }
        if (!skip)
        {
            Texture *newTexture = (Texture *)ArenaPush(arena, sizeof(Texture));
            newTexture->id = CreateTextureFromImage(path.C_Str(), type == aiTextureType_DIFFUSE);
            newTexture->type = GetTextureTypeFromAssimp(type);
            newTexture->hash = hash;
            texture->id = newTexture->id;
            texture->type = newTexture->type;
            texture->hash = newTexture->hash;
            loadedTextures->textures[loadedTextures->numTextures++] = *newTexture;
        }
        mesh->numTextures++;
    }
}

internal u64 GetTextureHandle(u32 textureID)
{
    u64 result = 0;

    if (textureID > 0)
    {
        result = glGetTextureHandleARB(textureID);
        if (!glIsTextureHandleResidentARB(result))
        {
            glMakeTextureHandleResidentARB(result);
        }
    }

    return result;
}

internal TextureHandles CreateTextureHandlesFromMaterial(Material *material)
{
    TextureHandles result;

    result.diffuseHandle = GetTextureHandle(material->diffuse.id);
    result.specularHandle = GetTextureHandle(material->specular.id);
    result.normalsHandle = GetTextureHandle(material->normals.id);
    result.displacementHandle = GetTextureHandle(material->displacement.id);

    return result;
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

internal Material CreateTextures(const char *diffusePath, const char *specularPath = "", const char *normalsPath = "",
                                 const char *displacementPath = "")
{
    Material result = {};
    result.diffuse = CreateTexture(diffusePath, TextureType::Diffuse);
    result.specular = CreateTexture(specularPath, TextureType::Specular);
    result.normals = CreateTexture(normalsPath, TextureType::Normals);
    result.displacement = CreateTexture(displacementPath, TextureType::Displacement);
    return result;
}
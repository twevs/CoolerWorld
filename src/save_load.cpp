#include "common.h"

internal void LoadShaderPass(FILE *file, ShaderProgram *shader)
{
    fread(&shader->numObjects, sizeof(u32), 1, file);
    fread(shader->objectIndices, sizeof(u32), shader->numObjects, file);
    fread(&shader->numModels, sizeof(u32), 1, file);
    fread(shader->modelIndices, sizeof(u32), shader->numModels, file);
}

internal bool LoadDrawingInfo(TransientDrawingInfo *transientInfo, PersistentDrawingInfo *info, CameraInfo *cameraInfo)
{
    FILE *file;
    errno_t opened = fopen_s(&file, "save.bin", "rb");
    if (opened != 0)
    {
        return false;
    }

    fread(info, sizeof(PersistentDrawingInfo), 1, file);
    transientInfo->numObjects = info->numObjects;
    for (u32 i = 0; i < transientInfo->numObjects; i++)
    {
        transientInfo->objects[i].position = info->objectPositions[i];
    }
    transientInfo->numModels = info->numModels;
    for (u32 i = 0; i < transientInfo->numModels; i++)
    {
        transientInfo->models[i].position = info->modelPositions[i];
    }
    LoadShaderPass(file, &transientInfo->gBufferShader);
    LoadShaderPass(file, &transientInfo->ssaoShader);
    LoadShaderPass(file, &transientInfo->ssaoBlurShader);
    LoadShaderPass(file, &transientInfo->dirDepthMapShader);
    LoadShaderPass(file, &transientInfo->spotDepthMapShader);
    LoadShaderPass(file, &transientInfo->pointDepthMapShader);
    LoadShaderPass(file, &transientInfo->instancedObjectShader);
    LoadShaderPass(file, &transientInfo->colorShader);
    LoadShaderPass(file, &transientInfo->outlineShader);
    LoadShaderPass(file, &transientInfo->glassShader);
    LoadShaderPass(file, &transientInfo->textureShader);
    LoadShaderPass(file, &transientInfo->postProcessShader);
    LoadShaderPass(file, &transientInfo->skyboxShader);
    LoadShaderPass(file, &transientInfo->geometryShader);
    fread(cameraInfo, sizeof(CameraInfo), 1, file);

    fclose(file);

    return true;
}

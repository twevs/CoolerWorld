#include "common.h"

internal bool CompileShader(u32 *shaderID, GLenum shaderType, const char *shaderFilename)
{

    HANDLE file = CreateFileA(shaderFilename,        // lpFileName,
                              GENERIC_READ,          // dwDesiredAccess,
                              FILE_SHARE_READ,       // dwShareMode,
                              0,                     // lpSecurityAttributes,
                              OPEN_EXISTING,         // dwCreationDisposition,
                              FILE_ATTRIBUTE_NORMAL, // dwFlagsAndAttributes,
                              0                      // hTemplateFile
    );
    u32 fileSize = GetFileSize(file, 0);
    char *fileBuffer = (char *)VirtualAlloc(0, fileSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    DWORD bytesRead;
    ReadFile(file, fileBuffer, fileSize, &bytesRead, 0);
    myAssert(fileSize == bytesRead);

    *shaderID = glCreateShader(shaderType);
    glShaderSource(*shaderID, 1, &fileBuffer, NULL);
    VirtualFree(fileBuffer, fileSize, MEM_RELEASE);
    glCompileShader(*shaderID);

    int success;
    char infoLog[1024];
    glGetShaderiv(*shaderID, GL_COMPILE_STATUS, &success);
    glGetShaderInfoLog(*shaderID, 1024, NULL, infoLog);
    DebugPrintA("Shader compilation infolog for %s: %s\n", shaderFilename, infoLog);
    if (!success || strstr(infoLog, "Warning"))
    {
        return false;
    }

    CloseHandle(file);
    return true;
}

internal bool CreateShaderProgram(ShaderProgram *program)
{
    u32 vertexShaderID = 0;
    if (!CompileShader(&vertexShaderID, GL_VERTEX_SHADER, program->vertexShaderFilename))
    {
        return false;
    }

    bool hasGeometryShader = strlen(program->geometryShaderFilename) > 0;
    u32 geometryShaderID = 0;
    if (hasGeometryShader && !CompileShader(&geometryShaderID, GL_GEOMETRY_SHADER, program->geometryShaderFilename))
    {
        return false;
    }

    u32 fragmentShaderID = 0;
    if (!CompileShader(&fragmentShaderID, GL_FRAGMENT_SHADER, program->fragmentShaderFilename))
    {
        return false;
    }

    u32 *programID = &program->id;
    if (*programID == 0)
    {
        *programID = glCreateProgram();
    }
    glAttachShader(*programID, vertexShaderID);
    if (hasGeometryShader)
    {
        glAttachShader(*programID, geometryShaderID);
    }
    glAttachShader(*programID, fragmentShaderID);
    glLinkProgram(*programID);

    int success;
    char infoLog[1024];
    glGetProgramiv(*programID, GL_LINK_STATUS, &success);
    glGetProgramInfoLog(*programID, 1024, NULL, infoLog);
    DebugPrintA("Shader program creation infolog: %s\n", infoLog);
    if (!success || strstr(infoLog, "Warning"))
    {
        return false;
    }

    glDeleteShader(vertexShaderID);
    glDeleteShader(fragmentShaderID);

    return true;
}

internal FILETIME GetFileTime(const char *filename)
{
    HANDLE file = CreateFileA(filename, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    FILETIME fileTime = {};
    GetFileTime(file, 0, 0, &fileTime);
    CloseHandle(file);

    return fileTime;
}

internal void SetShaderUniformInt(u32 shaderProgram, const char *uniformName, s32 value)
{
    glUniform1i(glGetUniformLocation(shaderProgram, uniformName), value);
}

internal void SetShaderUniformUint(u32 shaderProgram, const char *uniformName, u32 value)
{
    glUniform1ui(glGetUniformLocation(shaderProgram, uniformName), value);
}

internal void SetShaderUniformFloat(u32 shaderProgram, const char *uniformName, float value)
{
    glUniform1f(glGetUniformLocation(shaderProgram, uniformName), value);
}

internal void SetShaderUniformVec2(u32 shaderProgram, const char *uniformName, glm::vec2 vector)
{
    glUniform2fv(glGetUniformLocation(shaderProgram, uniformName), 1, glm::value_ptr(vector));
}

internal void SetShaderUniformVec3(u32 shaderProgram, const char *uniformName, glm::vec3 vector)
{
    glUniform3fv(glGetUniformLocation(shaderProgram, uniformName), 1, glm::value_ptr(vector));
}

internal void SetShaderUniformVec4(u32 shaderProgram, const char *uniformName, glm::vec4 vector)
{
    glUniform4fv(glGetUniformLocation(shaderProgram, uniformName), 1, glm::value_ptr(vector));
}

internal void SetShaderUniformMat3(u32 shaderProgram, const char *uniformName, glm::mat3 *matrix)
{
    glUniformMatrix3fv(glGetUniformLocation(shaderProgram, uniformName), 1, GL_FALSE, glm::value_ptr(*matrix));
}

internal void SetShaderUniformMat4(u32 shaderProgram, const char *uniformName, glm::mat4 *matrix)
{
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, uniformName), 1, GL_FALSE, glm::value_ptr(*matrix));
}

internal bool CreateShaderProgram(ShaderProgram *program, const char *vertexShaderFilename,
                                  const char *fragmentShaderFilename, const char *geometryShaderFilename = "")
{
    ShaderProgram newShaderProgram = {};
    strcpy_s(newShaderProgram.vertexShaderFilename, vertexShaderFilename);
    strcpy_s(newShaderProgram.geometryShaderFilename, geometryShaderFilename);
    strcpy_s(newShaderProgram.fragmentShaderFilename, fragmentShaderFilename);
    newShaderProgram.vertexShaderTime = GetFileTime(vertexShaderFilename);
    newShaderProgram.geometryShaderTime = GetFileTime(geometryShaderFilename);
    newShaderProgram.fragmentShaderTime = GetFileTime(fragmentShaderFilename);
    if (!CreateShaderProgram(&newShaderProgram))
    {
        return false;
    }
    if (glIsProgram(program->id))
    {
        memcpy(newShaderProgram.objectIndices, program->objectIndices, program->numObjects * sizeof(u32));
        newShaderProgram.numObjects = program->numObjects;
        memcpy(newShaderProgram.modelIndices, program->modelIndices, program->numModels * sizeof(u32));
        newShaderProgram.numModels = program->numModels;
        glDeleteProgram(program->id);
    }

    *program = newShaderProgram;
    return true;
}
#define SSAO_KERNEL_SIZE 64
#define SSAO_NOISE_SIZE 16

internal void GenerateSSAOSamplesAndNoise(TransientDrawingInfo *transientInfo)
{
    srand((u32)Win32GetWallClock());

    glm::vec3 ssaoKernel[SSAO_KERNEL_SIZE] = {};
    for (u32 i = 0; i < SSAO_KERNEL_SIZE; i++)
    {
        f32 xOffset = ((f32)rand() / RAND_MAX) * 2.f - 1.f;
        f32 yOffset = ((f32)rand() / RAND_MAX) * 2.f - 1.f;
        f32 zOffset = ((f32)rand() / RAND_MAX);
        glm::vec3 sample(xOffset, yOffset, zOffset);
        sample = glm::normalize(sample);

        sample *= ((f32)rand() / RAND_MAX);
        f32 scale = (f32)i / SSAO_KERNEL_SIZE;
        scale = lerp(.1f, 1.f, scale * scale);
        sample *= scale;

        ssaoKernel[i] = sample;
    }

    u32 ssaoShader = transientInfo->ssaoShader.id;
    glUseProgram(ssaoShader);
    glUniform3fv(glGetUniformLocation(ssaoShader, "samples"), SSAO_KERNEL_SIZE, (f32 *)ssaoKernel);

    glm::vec3 ssaoNoise[SSAO_NOISE_SIZE] = {};
    for (u32 i = 0; i < SSAO_NOISE_SIZE; i++)
    {
        f32 x = ((f32)rand() / RAND_MAX) * 2.f - 1.f;
        f32 y = ((f32)rand() / RAND_MAX) * 2.f - 1.f;
        glm::vec3 sample(x, y, 0.f);
        sample = glm::normalize(sample);

        ssaoNoise[i] = sample;
    }

    if (glIsTexture(transientInfo->ssaoNoiseTexture))
    {
        glDeleteTextures(1, &transientInfo->ssaoNoiseTexture);
    }
    u32 *ssaoNoiseTexture = &transientInfo->ssaoNoiseTexture;
    glCreateTextures(GL_TEXTURE_2D, 1, ssaoNoiseTexture);
    u32 sideLength = (u32)(sqrtf(SSAO_NOISE_SIZE));
    glTextureStorage2D(*ssaoNoiseTexture, 1, GL_RGBA16F, sideLength, sideLength);
    glTextureSubImage2D(*ssaoNoiseTexture, 0, 0, 0, sideLength, sideLength, GL_RGB, GL_FLOAT, &ssaoNoise);
    glTextureParameteri(*ssaoNoiseTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(*ssaoNoiseTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(*ssaoNoiseTexture, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(*ssaoNoiseTexture, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

internal bool CreateShaderPrograms(TransientDrawingInfo *info)
{
    if (!CreateShaderProgram(&info->gBufferShader, "gbuffer.vs", "gbuffer.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->ssaoShader, "vertex_shader.vs", "ssao.fs"))
    {
        return false;
    }
    else
    {
        GenerateSSAOSamplesAndNoise(info);
    }
    if (!CreateShaderProgram(&info->ssaoBlurShader, "vertex_shader.vs", "ssao_blur.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->nonPointLightingShader, "vertex_shader.vs", "fragment_shader.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->pointLightingShader, "vertex_shader.vs", "point_lighting.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->dirDepthMapShader, "depth_map.vs", "depth_map.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->spotDepthMapShader, "spot_depth_map.vs", "depth_map.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->pointDepthMapShader, "depth_cube_map.vs", "depth_cube_map.fs", "depth_cube_map.gs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->instancedObjectShader, "instanced.vs", "gbuffer.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->colorShader, "vertex_shader.vs", "color.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->outlineShader, "vertex_shader.vs", "outline.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->glassShader, "vertex_shader.vs", "glass.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->textureShader, "vertex_shader.vs", "texture.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->postProcessShader, "vertex_shader.vs", "postprocess.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->skyboxShader, "cubemap.vs", "cubemap.fs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->geometryShader, "vertex_shader_geometry.vs", "color.fs", "vis_normals.gs"))
    {
        return false;
    }
    if (!CreateShaderProgram(&info->gaussianShader, "vertex_shader.vs", "gaussian.fs"))
    {
        return false;
    }

    return true;
}

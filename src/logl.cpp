#include "common.h"

global_variable ImGuiContext *imGuiContext;
global_variable ImGuiIO *imGuiIO;

extern "C" __declspec(dllexport)
void InitializeImGuiInModule(HWND window)
{
    IMGUI_CHECKVERSION();
    imGuiContext = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    imGuiIO = &io;
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(window);
    ImGui_ImplOpenGL3_Init("#version 330");
}

extern "C" __declspec(dllexport)
ImGuiIO *GetImGuiIO()
{
  return imGuiIO;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
  HWND hWnd,
  UINT msg,
  WPARAM wParam,
  LPARAM lParam);

extern "C" __declspec(dllexport)
LRESULT ImGui_WndProcHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  return ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
}

extern "C" __declspec(dllexport)
void PrintDepthTestFunc(u32 val, char *outputBuffer, u32 bufSize)
{
    switch (val)
    {
        case GL_NEVER:
            sprintf_s(outputBuffer, bufSize, "GL_NEVER\n");
            break;
        case GL_LESS:
            sprintf_s(outputBuffer, bufSize, "GL_LESS\n");
            break;
        case GL_EQUAL:
            sprintf_s(outputBuffer, bufSize, "GL_EQUAL\n");
            break;
        case GL_LEQUAL:
            sprintf_s(outputBuffer, bufSize, "GL_LEQUAL\n");
            break;
        case GL_GREATER:
            sprintf_s(outputBuffer, bufSize, "GL_GREATER\n");
            break;
        case GL_NOTEQUAL:
            sprintf_s(outputBuffer, bufSize, "GL_NOTEQUAL\n");
            break;
        case GL_GEQUAL:
            sprintf_s(outputBuffer, bufSize, "GL_GEQUAL\n");
            break;
        case GL_ALWAYS:
            sprintf_s(outputBuffer, bufSize, "GL_ALWAYS\n");
            break;
        default:
            myAssert(false);
            break;
    }
}

internal glm::mat4 LookAt(
     CameraInfo *cameraInfo,
     glm::vec3 cameraTarget,
     glm::vec3 cameraUpVector,
     float farPlaneDistance)
{
    glm::mat4 inverseTranslation = glm::mat4(1.f);
    inverseTranslation[3][0] = -cameraInfo->pos.x;
    inverseTranslation[3][1] = -cameraInfo->pos.y;
    inverseTranslation[3][2] = -cameraInfo->pos.z;
    
    glm::vec3 direction = glm::normalize(cameraInfo->pos - cameraTarget);
    glm::vec3 rightVector = glm::normalize(glm::cross(cameraUpVector, direction));
    glm::vec3 upVector = (glm::cross(direction, rightVector));
    
    // TODO: investigate why filling the last column with the negative of cameraPosition does not
    // produce the same effect as the multiplication by the inverse translation.
    glm::mat4 inverseRotation = glm::mat4(1.f);
    inverseRotation[0][0] = rightVector.x;
    inverseRotation[1][0] = rightVector.y;
    inverseRotation[2][0] = rightVector.z;
    inverseRotation[0][1] = upVector.x;
    inverseRotation[1][1] = upVector.y;
    inverseRotation[2][1] = upVector.z;
    inverseRotation[0][2] = direction.x;
    inverseRotation[1][2] = direction.y;
    inverseRotation[2][2] = direction.z;
    
    // TODO: figure out what the deal is with the inverse scaling matrix in "Computer graphics:
    // Principles and practice", p. 306. I'm leaving this unused for now as it breaks rendering.
    glm::mat4 inverseScale =
    {
        1.f / (farPlaneDistance * (tanf(cameraInfo->fov / cameraInfo->aspectRatio * PI / 180.f) / 2.f)), 0.f, 0.f, 0.f,
        0.f, 1.f / (farPlaneDistance * (tanf(cameraInfo->fov * PI / 180.f) / 2.f)), 0.f, 0.f,
        0.f, 0.f, 1.f / farPlaneDistance, 0.f,
        0.f, 0.f, 0.f, 1.f
    };
    
    return inverseRotation * inverseTranslation;
}

extern "C" __declspec(dllexport)
void SetShaderUniformSampler(u32 shaderProgram, const char *uniformName, u32 slot)
{
    glUniform1i(glGetUniformLocation(shaderProgram, uniformName), slot);
}

internal void SetShaderUniformFloat(u32 shaderProgram, const char *uniformName, float value)
{
    glUniform1f(glGetUniformLocation(shaderProgram, uniformName), value);
}

internal void SetShaderUniformVec3(u32 shaderProgram, const char *uniformName, glm::vec3 vector)
{
    glUniform3fv(glGetUniformLocation(shaderProgram, uniformName), 1, glm::value_ptr(vector));
}

internal void SetShaderUniformMat3(u32 shaderProgram, const char *uniformName, glm::mat3* matrix)
{
    glUniformMatrix3fv(glGetUniformLocation(shaderProgram, uniformName), 1, GL_FALSE, glm::value_ptr(*matrix));
}

internal void SetShaderUniformMat4(u32 shaderProgram, const char *uniformName, glm::mat4* matrix)
{
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, uniformName), 1, GL_FALSE, glm::value_ptr(*matrix));
}

extern "C" __declspec(dllexport)
void SaveDrawingInfo(DrawingInfo *info, CameraInfo *cameraInfo)
{
    FILE *file;
    fopen_s(&file, "save.bin", "wb");
    
    fwrite(&cameraInfo->pos, sizeof(cameraInfo->pos), 1, file);
    fwrite(&cameraInfo->pitch, sizeof(cameraInfo->pitch), 1, file);
    fwrite(&cameraInfo->yaw, sizeof(cameraInfo->yaw), 1, file);
    fwrite(&info->wireframeMode, sizeof(info->wireframeMode), 1, file);
    fwrite(&info->depthTestFunc, sizeof(info->depthTestFunc), 1, file);
    fwrite(&info->clearColor, sizeof(info->clearColor), 1, file);
    fwrite(&info->dirLight, sizeof(DirLight), 1, file);
    fwrite(&info->pointLights, sizeof(PointLight), NUM_POINTLIGHTS, file);
    fwrite(&info->spotLight, sizeof(SpotLight), 1, file);
    
    fclose(file);
}

extern "C" __declspec(dllexport)
bool LoadDrawingInfo(DrawingInfo *info, CameraInfo *cameraInfo)
{
    FILE *file;
    errno_t opened = fopen_s(&file, "save.bin", "rb");
    if (opened != 0)
    {
        return false;
    }
    
    fread(&cameraInfo->pos, sizeof(cameraInfo->pos), 1, file);
    fread(&cameraInfo->pitch, sizeof(cameraInfo->pitch), 1, file);
    fread(&cameraInfo->yaw, sizeof(cameraInfo->yaw), 1, file);
    fread(&info->wireframeMode, sizeof(info->wireframeMode), 1, file);
    glPolygonMode(GL_FRONT_AND_BACK, info->wireframeMode ? GL_LINE : GL_FILL);
    fread(&info->depthTestFunc, sizeof(info->depthTestFunc), 1, file);
    glDepthFunc(info->depthTestFunc);
    fread(&info->clearColor, sizeof(info->clearColor), 1, file);
    fread(&info->dirLight, sizeof(DirLight), 1, file);
    fread(&info->pointLights, sizeof(PointLight), NUM_POINTLIGHTS, file);
    fread(&info->spotLight, sizeof(SpotLight), 1, file);
    
    fclose(file);
    
    return true;
}

internal glm::mat4 GetCameraWorldRotation(CameraInfo *cameraInfo)
{
    // We are rotating in world space so this returns a world-space rotation.
    glm::vec3 cameraYawAxis = glm::vec3(0.f, 1.f, 0.f);
    glm::vec3 cameraPitchAxis = glm::vec3(1.f, 0.f, 0.f);
    
    glm::mat4 cameraYaw = glm::rotate(glm::mat4(1.f), cameraInfo->yaw, cameraYawAxis);
    glm::mat4 cameraPitch = glm::rotate(glm::mat4(1.f), cameraInfo->pitch, cameraPitchAxis);
    glm::mat4 cameraRotation = cameraYaw * cameraPitch;
    
    return cameraRotation;
}

internal glm::vec3 GetCameraRightVector(CameraInfo *cameraInfo)
{
    // World-space rotation * world-space axis -> world-space value.
    glm::vec4 cameraRightVec = GetCameraWorldRotation(cameraInfo) * glm::vec4(1.f, 0.f, 0.f, 0.f);
    
    return glm::vec3(cameraRightVec);
}

internal glm::vec3 GetCameraForwardVector(CameraInfo *cameraInfo)
{
    glm::vec4 cameraForwardVec = GetCameraWorldRotation(cameraInfo) * glm::vec4(0.f, 0.f, -1.f, 0.f);
    
    return glm::vec3(cameraForwardVec);
}

extern "C" __declspec(dllexport)
void ProvideCameraVectors(CameraInfo *cameraInfo)
{
    cameraInfo->forwardVector = GetCameraForwardVector(cameraInfo);
    cameraInfo->rightVector = GetCameraRightVector(cameraInfo);
};

extern "C" __declspec(dllexport)
void DrawWindow(
  HWND window,
  HDC hdc,
  bool *running,
  DrawingInfo *drawingInfo,
  CameraInfo *cameraInfo)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
  
    if (!drawingInfo->initialized)
    {
        return;
    }
    
    float *cc = drawingInfo->clearColor;
    // glClearColor(0.f, 1.f, 0.f, 1.f);
    glClearColor(cc[0], cc[1], cc[2], cc[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  
    // The camera target should always be camera pos + local forward vector.
    // We want to be able to rotate the camera, however.
    // How do we obtain the forward vector? Translate world forward vec by camera world rotation matrix.
    glm::vec3 cameraForwardVec = glm::normalize(GetCameraForwardVector(cameraInfo));
    
    glm::vec3 cameraTarget = cameraInfo->pos + cameraForwardVec;
        
    // The camera direction vector points from the camera target to the camera itself, maintaining
    // the OpenGL convention of the Z-axis being positive towards the viewer.
    glm::vec3 cameraDirection = glm::normalize(cameraInfo->pos - cameraTarget);
    
    glm::vec3 upVector = glm::vec3(0.f, 1.f, 0.f);
    glm::vec3 cameraRightVec = glm::normalize(glm::cross(upVector, cameraDirection));
    glm::vec3 cameraUpVec = glm::normalize(glm::cross(cameraDirection, cameraRightVec));
    
    float farPlaneDistance = 100.f;
    glm::mat4 viewMatrix = LookAt(cameraInfo, cameraTarget, cameraUpVec, farPlaneDistance);
    
    // Projection matrix: transforms vertices from view space to clip space.
    glm::mat4 projectionMatrix = glm::perspective(
        glm::radians(cameraInfo->fov), 
        cameraInfo->aspectRatio, 
        .1f, 
        farPlaneDistance);
  
    // Point lights.
    {
        u32 shaderProgram = drawingInfo->lightShaderProgram;
        glUseProgram(shaderProgram);
        
        SetShaderUniformMat4(shaderProgram, "viewMatrix", &viewMatrix);
        SetShaderUniformMat4(shaderProgram, "projectionMatrix", &projectionMatrix);
        
        glBindVertexArray(drawingInfo->lightVao);
        
        for (u32 lightIndex = 0; lightIndex < NUM_POINTLIGHTS; lightIndex++)
        {
            PointLight *curLight = &drawingInfo->pointLights[lightIndex];
            // Model matrix: transforms vertices from local to world space.
            glm::mat4 modelMatrix = glm::mat4(1.f);
            modelMatrix = glm::translate(modelMatrix, curLight->position);
      
            SetShaderUniformVec3(drawingInfo->lightShaderProgram, "lightColor", curLight->diffuse);
            SetShaderUniformMat4(shaderProgram, "modelMatrix", &modelMatrix);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }
    }
  
    // Object.
    {
        u32 shaderProgram = drawingInfo->objectShaderProgram;
        glUseProgram(shaderProgram);
        
        SetShaderUniformFloat(shaderProgram, "material.shininess", 32.f);
        
        SetShaderUniformVec3(shaderProgram, "dirLight.direction", drawingInfo->dirLight.direction);
        SetShaderUniformVec3(shaderProgram, "dirLight.ambient", drawingInfo->dirLight.ambient);
        SetShaderUniformVec3(shaderProgram, "dirLight.diffuse", drawingInfo->dirLight.diffuse);
        SetShaderUniformVec3(shaderProgram, "dirLight.specular", drawingInfo->dirLight.specular);
        
        for (u32 lightIndex = 0; lightIndex < NUM_POINTLIGHTS; lightIndex++)
        {
            PointLight *lights = drawingInfo->pointLights;
            PointLight light = lights[lightIndex];
            
            char uniformString[32];
            sprintf_s(uniformString, "pointLights[%i].position", lightIndex);
            SetShaderUniformVec3(shaderProgram, uniformString, light.position);
            sprintf_s(uniformString, "pointLights[%i].ambient", lightIndex);
            SetShaderUniformVec3(shaderProgram, uniformString, light.ambient);
            sprintf_s(uniformString, "pointLights[%i].diffuse", lightIndex);
            SetShaderUniformVec3(shaderProgram, uniformString, light.diffuse);
            sprintf_s(uniformString, "pointLights[%i].specular", lightIndex);
            SetShaderUniformVec3(shaderProgram, uniformString, light.specular);
            
            Attenuation *att = &globalAttenuationTable[light.attIndex];
            sprintf_s(uniformString, "pointLights[%i].linear", lightIndex);
            SetShaderUniformFloat(shaderProgram, uniformString, att->linear);
            sprintf_s(uniformString, "pointLights[%i].quadratic", lightIndex);
            SetShaderUniformFloat(shaderProgram, uniformString, att->quadratic);
        }
        
        SetShaderUniformVec3(shaderProgram, "spotLight.position", cameraInfo->pos);
        SetShaderUniformVec3(shaderProgram, "spotLight.direction", GetCameraForwardVector(cameraInfo));
        SetShaderUniformFloat(shaderProgram, "spotLight.innerCutoff", cosf(drawingInfo->spotLight.innerCutoff));
        SetShaderUniformFloat(shaderProgram, "spotLight.outerCutoff", cosf(drawingInfo->spotLight.outerCutoff));
        SetShaderUniformVec3(shaderProgram, "spotLight.ambient", drawingInfo->spotLight.ambient);
        SetShaderUniformVec3(shaderProgram, "spotLight.diffuse", drawingInfo->spotLight.diffuse);
        SetShaderUniformVec3(shaderProgram, "spotLight.specular", drawingInfo->spotLight.specular);
        
        SetShaderUniformVec3(shaderProgram, "cameraPos", cameraInfo->pos);
        
        Model *model = &drawingInfo->backpack;
        for (u32 i = 0; i < model->meshCount; i++)
        {
            glBindVertexArray(model->vaos[i]);
    
            Mesh *mesh = &drawingInfo->backpack.meshes[i];
            if (mesh->numTextures > 0)
            {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, mesh->textures[0].id);
            }
            if (mesh->numTextures > 1)
            {
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, mesh->textures[1].id);
            }
            
            // Model matrix: transforms vertices from local to world space.
            glm::mat4 modelMatrix = glm::mat4(1.f);
            modelMatrix = glm::translate(modelMatrix, glm::vec3(0.f));
        
            glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(modelMatrix)));

            SetShaderUniformMat4(shaderProgram, "modelMatrix", &modelMatrix);
            SetShaderUniformMat3(shaderProgram, "normalMatrix", &normalMatrix);
            SetShaderUniformMat4(shaderProgram, "viewMatrix", &viewMatrix);
            SetShaderUniformMat4(shaderProgram, "projectionMatrix", &projectionMatrix);
            glDrawElements(GL_TRIANGLES, mesh->verticesSize / sizeof(Vertex), GL_UNSIGNED_INT, 0);
        }
    }
    
    u32 id = 0;
    ImGui::Begin("Debug Window");

    char depthTestFuncStr[16];
    PrintDepthTestFunc(drawingInfo->depthTestFunc, depthTestFuncStr, sizeof(depthTestFuncStr));
    ImGui::Text("Depth-test function: %s", depthTestFuncStr);
  
    ImGui::SliderFloat3("Camera position", glm::value_ptr(cameraInfo->pos), -150.f, 150.f);
    ImGui::SliderFloat2("Camera rotation", &cameraInfo->yaw, -PI, PI);
    if (ImGui::Button("Reset camera"))
    {
        cameraInfo->pos = glm::vec3(0.f);
        cameraInfo->yaw = 0.f;
        cameraInfo->pitch = 0.f;
    }

    ImGui::Separator();

    ImGui::SliderFloat4("Clear color", drawingInfo->clearColor, 0.f, 1.f);

    if (ImGui::CollapsingHeader("Directional light"))
    {
        ImGui::PushID(id++);
        ImGui::SliderFloat3("Direction", glm::value_ptr(drawingInfo->dirLight.direction), -10.f, 10.f);
        ImGui::SliderFloat3("Ambient", glm::value_ptr(drawingInfo->dirLight.ambient), 0.f, 1.f);
        ImGui::SliderFloat3("Diffuse", glm::value_ptr(drawingInfo->dirLight.diffuse), 0.f, 1.f);
        ImGui::SliderFloat3("Specular", glm::value_ptr(drawingInfo->dirLight.specular), 0.f, 1.f);
        ImGui::PopID();
    }

    if (ImGui::CollapsingHeader("Point lights"))
    {
        PointLight *lights = drawingInfo->pointLights;
        for (u32 index = 0; index < NUM_POINTLIGHTS; index++)
        {
            ImGui::PushID(id++);
            char treeName[32];
            sprintf_s(treeName, "Light #%i", index);
            if (ImGui::TreeNode(treeName))
            {
                ImGui::InputFloat3("Position", glm::value_ptr(lights[index].position));
                // ImGui::SliderFloat3("Position", glm::value_ptr(lights[index].position), -150.f, 150.f);
                ImGui::SliderFloat3("Ambient", glm::value_ptr(lights[index].ambient), 0.f, 1.f);
                ImGui::SliderFloat3("Diffuse", glm::value_ptr(lights[index].diffuse), 0.f, 1.f);
                ImGui::SliderFloat3("Specular", glm::value_ptr(lights[index].specular), 0.f, 1.f);
                ImGui::SliderInt("Attenuation", &lights[index].attIndex, 0, myArraySize(globalAttenuationTable) - 1);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }

    if (ImGui::CollapsingHeader("Spot light"))
    {
        ImGui::PushID(id++);
        ImGui::SliderFloat3("Position", glm::value_ptr(drawingInfo->spotLight.position), -10.f, 10.f);
        ImGui::SliderFloat3("Direction", glm::value_ptr(drawingInfo->spotLight.direction), -10.f, 10.f);
        ImGui::SliderFloat3("Ambient", glm::value_ptr(drawingInfo->spotLight.ambient), 0.f, 1.f);
        ImGui::SliderFloat3("Diffuse", glm::value_ptr(drawingInfo->spotLight.diffuse), 0.f, 1.f);
        ImGui::SliderFloat3("Specular", glm::value_ptr(drawingInfo->spotLight.specular), 0.f, 1.f);
        ImGui::SliderFloat("Inner cutoff", &drawingInfo->spotLight.innerCutoff, 0.f, PI / 2.f);
        ImGui::SliderFloat("Outer cutoff", &drawingInfo->spotLight.outerCutoff, 0.f, PI / 2.f);
        ImGui::PopID();
    }

    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (!SwapBuffers(hdc))
    {
        if (MessageBoxW(window, L"Failed to swap buffers", L"OpenGL error", MB_OK) == S_OK)
        {
            *running = false;
        }
    }
}


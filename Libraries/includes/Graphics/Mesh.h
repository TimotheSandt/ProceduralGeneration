#pragma once

#include <vector>
#include <array>
#include <memory>
#include <unordered_map>
#include <string>
#include <cstdint>

#include "Logger.h"

#include "Buffer.h"
#include "Graphics/Core/GraphicsResources.h"
#include "Texture.h"
#include "ShaderProgram.h"
#include "Camera.h"

class Mesh
{
  public:
    Mesh() = default;
    Mesh(std::vector<float> vertices, std::vector<std::uint32_t> indices, std::vector<std::uint32_t> sizeAttrib);
    Mesh(std::vector<float> vertices, std::vector<std::uint32_t> indices, std::vector<std::uint32_t> sizeAttrib,
         std::vector<float> instances, std::vector<std::uint32_t> sizeAttribInstance);

    Mesh(const Mesh &);
    Mesh &operator=(const Mesh &);

    Mesh(Mesh &&) noexcept;
    Mesh &operator=(Mesh &&) noexcept;

    ~Mesh() { this->Destroy(); }

    void Initialize(std::vector<float> vertices, std::vector<std::uint32_t> indices, std::vector<std::uint32_t> sizeAttrib);
    void Initialize(std::vector<float> vertices, std::vector<std::uint32_t> indices, std::vector<std::uint32_t> sizeAttrib,
                    std::vector<float> instances, std::vector<std::uint32_t> sizeAttribInstance);
    void Destroy();

    void AddTexture(Texture texture);
    void AddTexture(const char *image, const char *name, TextureFormat format, TexturePixelType pixelType);
    void SetShader(ShaderProgram &shaderProgram) { this->shaderProgram = std::move(shaderProgram); }
    void SetShaderCopy(const ShaderProgram &shaderProgram) { this->shaderProgram = ShaderProgram(shaderProgram); }
    void SetShader(const char *vertexPath, const char *fragmentPath) { this->shaderProgram.SetShader(vertexPath, fragmentPath); }
    void SetPosition(glm::vec3 position) { this->position = position; }
    void SetScale(glm::vec3 scale) { this->scale = scale; }
    void SetRotation(glm::vec3 rotation) { this->rotation = rotation; }

    void UpdateUBO();

    void InitUniform4f(const char *uniform, const float *data);
    void InitUniform3f(const char *uniform, const float *data);
    void InitUniform2f(const char *uniform, const float *data);
    void InitUniform1f(const char *uniform, const float *data);
    void InitUniform4i(const char *uniform, const int *data);
    void InitUniform3i(const char *uniform, const int *data);
    void InitUniform2i(const char *uniform, const int *data);
    void InitUniform1i(const char *uniform, const int *data);
    void InitUniformMatrix4f(const char *uniform, const float *data);

    void Render(Camera &camera);
    void Draw(bool wireframe = false) const;

    // Bind/Unbind for custom rendering (UI)
    void BindShader() { shaderProgram.Bind(); }
    void UnbindShader() { shaderProgram.Unbind(); }
    void BindVAO()
    {
        if (geometry != nullptr)
        {
            geometry->Bind();
        }
    }
    void UnbindVAO()
    {
        if (geometry != nullptr)
        {
            geometry->Unbind();
        }
    }
    bool IsShaderCompiled() const { return shaderProgram.IsCompiled(); }

    glm::vec3 &GetPosition() { return this->position; }
    glm::vec3 &GetScale() { return this->scale; }
    glm::vec3 &GetRotation() { return this->rotation; }

  private:
    std::vector<float> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<std::uint32_t> sizeAttrib;
    std::vector<Texture> textures;
    ShaderProgram shaderProgram;

    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    glm::vec3 rotation = glm::vec3(0.0f);

    std::uint32_t instancing = 1;
    std::vector<float> instances;
    std::vector<std::uint32_t> sizeAttribInstance;

    std::unique_ptr<IGeometryResource> geometry;
    Buffer modelBuffer;

  private:
    struct UniformCache
    {
        std::array<uint8_t, 64> data; // Up to mat4
        size_t size;
        int location;
        std::uint32_t shaderProgramID;

        UniformCache() : data({0}), size(0), location(-2), shaderProgramID(0) {}
    };
    std::unique_ptr<std::unordered_map<std::string, UniformCache>> uniformCache;
    std::unordered_map<std::string, UniformCache> &GetOrCreateUniformCache();
    bool CacheUniform(const std::string &uniform, void *data, size_t size);
    int CachedUniformLocation(const std::string &uniform);
    void FreeCache();
    void Swap(Mesh &other) noexcept;
};

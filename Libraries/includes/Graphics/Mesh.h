#pragma once

#include <vector>
#include <array>
#include <unordered_map>
#include <string>

#include "Logger.h"

#include "VAO.h"
#include "VBO.h"
#include "EBO.h"
#include "UBO.h"
#include "Texture.h"
#include "Shader.h"
#include "Camera.h"

class Mesh
{
public:
    Mesh() = default;
    Mesh(std::vector<GLfloat> vertices, std::vector<GLuint> indices, std::vector<GLuint> sizeAttrib);
    Mesh(std::vector<GLfloat> vertices, std::vector<GLuint> indices, std::vector<GLuint> sizeAttrib,
        std::vector<GLfloat> instances, std::vector<GLuint> SizeAttribInstance);

    Mesh(const Mesh&) noexcept;
    Mesh operator=(const Mesh&) noexcept;

    Mesh(Mesh&&) noexcept;
    Mesh operator=(Mesh&&) noexcept;

    ~Mesh() { this->Destroy(); }

    void Initialize(std::vector<GLfloat> vertices, std::vector<GLuint> indices, std::vector<GLuint> sizeAttrib);
    void Initialize(std::vector<GLfloat> vertices, std::vector<GLuint> indices, std::vector<GLuint> sizeAttrib,
                    std::vector<GLfloat> instances, std::vector<GLuint> SizeAttribInstance);
    void Destroy();

    void AddTexture(Texture texture);
    void AddTexture(const char* image, const char* name, GLenum format, GLenum pixelType);
    void SetShader(Shader& shader) { this->shader = std::move(shader); }
    void SetShaderCopy(const Shader& shader) { this->shader = Shader(shader); }
    void SetShader(const char* vertexPath, const char* fragmentPath) { this->shader.SetShader(vertexPath, fragmentPath); }
    void SetPosition(glm::vec3 position) { this->position = position; }
    void SetScale(glm::vec3 scale) { this->scale = scale; }
    void SetRotation(glm::vec3 rotation) { this->rotation = rotation; }

    void UpdateUBO();

    void InitUniform4f(const char* uniform, const GLfloat* data);
    void InitUniform3f(const char* uniform, const GLfloat* data);
    void InitUniform2f(const char* uniform, const GLfloat* data);
    void InitUniform1f(const char* uniform, const GLfloat* data);
    void InitUniform4i(const char* uniform, const GLint* data);
    void InitUniform3i(const char* uniform, const GLint* data);
    void InitUniform2i(const char* uniform, const GLint* data);
    void InitUniform1i(const char* uniform, const GLint* data);
    void InitUniformMatrix4f(const char* uniform, const GLfloat* data);

    void Render(Camera& camera);
    void Draw(bool wireframe = false) const;

    // Bind/Unbind for custom rendering (UI)
    void BindShader() { shader.Bind(); }
    void UnbindShader() { shader.Unbind(); }
    void BindVAO() { bVAO.Bind(); }
    void UnbindVAO() { bVAO.Unbind(); }
    bool IsShaderCompiled() const { return shader.IsCompiled(); }

    glm::vec3& GetPosition() { return this->position; }
    glm::vec3& GetScale() { return this->scale; }
    glm::vec3& GetRotation() { return this->rotation; }

private:
    std::vector<GLfloat> vertices;
    std::vector<GLuint> indices;
    std::vector<GLuint> sizeAttrib;
    std::vector<Texture> textures;
    Shader shader;

    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    glm::vec3 rotation = glm::vec3(0.0f);

    GLuint instancing;
    std::vector<GLfloat> instances;
    std::vector<GLuint> SizeAttribInstance;

    VAO bVAO;
    UBO bUBO;


private:
    struct UniformCache {
        std::array<uint8_t, 64> data; // Up to mat4
        size_t size;
        GLint location;
        GLuint shaderID;

        UniformCache() : data({0}), size(0), location(-2), shaderID(0) {}
    };
    std::unordered_map<std::string, UniformCache> uniformCache {};
    bool CacheUniform(const std::string& uniform, void* data, size_t size);
    GLint CachedUniformLocation(const std::string& uniform);
    void FreeCache();
    void Swap(Mesh& other) noexcept;
};

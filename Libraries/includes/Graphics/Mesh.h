#pragma once

#include <vector>
#include <string>

#include "VAO.h"
#include "VBO.h"
#include "EBO.h"
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


    void Initialize(std::vector<GLfloat> vertices, std::vector<GLuint> indices, std::vector<GLuint> sizeAttrib);
    void Initialize(std::vector<GLfloat> vertices, std::vector<GLuint> indices, std::vector<GLuint> sizeAttrib,
                    std::vector<GLfloat> instances, std::vector<GLuint> SizeAttribInstance);
    void Destroy();

    void AddTexture(Texture texture);
    void AddTexture(const char* image, const char* name, GLenum format, GLenum pixelType);
    void SetShader(Shader shader) { this->shader = shader; }
    void SetShader(const char* vertexPath, const char* fragmentPath) { this->shader.SetShader(vertexPath, fragmentPath); }
    void SetPosition(glm::vec3 position) { this->position = position; }
    void SetScale(glm::vec3 scale) { this->scale = scale; }
    void SetRotation(glm::vec3 rotation) { this->rotation = rotation; }

    void InitUniform();
    void InitUniform4f(const char* uniform, const GLfloat* data);
    void InitUniform3f(const char* uniform, const GLfloat* data);
    void InitUniform2f(const char* uniform, const GLfloat* data);
    void InitUniform1f(const char* uniform, const GLfloat* data);
    void InitUniform4i(const char* uniform, const GLint* data);
    void InitUniform3i(const char* uniform, const GLint* data);
    void InitUniform2i(const char* uniform, const GLint* data);
    void InitUniform1i(const char* uniform, const GLint* data);
    void InitUniformMatrix4f(const char* uniform, const GLfloat* data);


    void Draw(Camera& camera);

    glm::vec3& GetPosition() { return this->position; }
    glm::vec3& GetScale() { return this->scale; }
    glm::vec3& GetRotation() { return this->rotation; }

private:
    std::vector<GLfloat> vertices;
    std::vector<GLuint> indices;
    std::vector<Texture> textures;
    Shader shader;

    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    glm::vec3 rotation = glm::vec3(0.0f);

    GLuint instancing;

    VAO VAO;
};
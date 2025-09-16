#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>
#include <vector>

class VBO
{
public:
    VBO();
    VBO(std::vector<GLfloat>& vertices);
    VBO(std::vector<glm::mat4>& mat4);
    ~VBO();

    VBO(const VBO&) = delete;
    VBO& operator=(const VBO&) = delete;

    VBO(VBO&& other) noexcept;
    VBO& operator=(VBO&& other) noexcept;

    void Initialize(std::vector<GLfloat>& vertices);
    void Initialize(std::vector<glm::mat4>& mat4);

    void Bind();
    void Unbind();
    void Destroy();
    void UploadData(const void* data, GLsizeiptr size);

private:
    GLuint ID;
};
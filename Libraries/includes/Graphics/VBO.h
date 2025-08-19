#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>
#include <vector>

class VBO
{
public:
    VBO(std::vector<GLfloat>& vertices);
    VBO(std::vector<glm::mat4>& mat4);
    ~VBO();

    void Bind();
    void Unbind();
    void Destroy();

private:
    GLuint ID;
};
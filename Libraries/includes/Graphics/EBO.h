#pragma once

#include <glad/glad.h>
#include <vector>

class EBO
{
public:
    EBO(std::vector<GLuint>& indices);
    ~EBO();

    void Bind();
    void Unbind();
    void Destroy();

private:
    GLuint ID;
};
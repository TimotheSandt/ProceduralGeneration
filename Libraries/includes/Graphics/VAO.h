#pragma once

#include <glad/glad.h>
#include "VBO.h"

class VAO
{
public:
    VAO();

    void Generate();

	void LinkAttrib(VBO& VBO, GLuint layout, GLuint numComponents, GLenum type, GLsizeiptr stride, void* offset);
    void Bind();
    void Unbind();
    void Destroy();

private:
    GLuint ID;
};
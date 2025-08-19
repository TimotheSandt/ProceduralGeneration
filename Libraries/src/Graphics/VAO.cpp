#include "VAO.h"

VAO::VAO()
{
    glGenVertexArrays(1, &this->ID);
}

void VAO::Generate()
{
    glDeleteVertexArrays(1, &this->ID);
    glGenVertexArrays(1, &this->ID);
}

void VAO::LinkAttrib(VBO& VBO, GLuint layout, GLuint numComponents, GLenum type, GLsizeiptr stride, void* offset)
{
    VBO.Bind();
    glVertexAttribPointer(layout, numComponents, type, GL_FALSE, stride, offset);
    glEnableVertexAttribArray(layout);
    VBO.Unbind();
}

void VAO::Bind()
{
    glBindVertexArray(this->ID);
}

void VAO::Unbind()
{
    glBindVertexArray(0);
}

void VAO::Destroy()
{
    glDeleteVertexArrays(1, &this->ID);
}
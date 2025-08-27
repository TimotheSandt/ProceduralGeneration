#include "EBO.h"

EBO::EBO(std::vector<GLuint>& indices)
{
    glGenBuffers(1, &this->ID);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->ID);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);
}

EBO::~EBO()
{
    this->Destroy();
}

void EBO::Bind()
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->ID);
}

void EBO::Unbind()
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void EBO::Destroy()
{
    if (this->ID != 0) glDeleteBuffers(1, &this->ID);
    this->ID = 0;
}
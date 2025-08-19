#include "VBO.h"

VBO::VBO(std::vector<GLfloat>& vertices)
{
    glGenBuffers(1, &this->ID);
    glBindBuffer(GL_ARRAY_BUFFER, this->ID);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLuint), vertices.data(), GL_STATIC_DRAW);
}

VBO::VBO(std::vector<glm::mat4>& mat4)
{
    glGenBuffers(1, &this->ID);
    glBindBuffer(GL_ARRAY_BUFFER, this->ID);
    glBufferData(GL_ARRAY_BUFFER, mat4.size() * sizeof(glm::mat4), mat4.data(), GL_STATIC_DRAW);
}

VBO::~VBO()
{
    this->Destroy();
}

void VBO::Bind()
{
    glBindBuffer(GL_ARRAY_BUFFER, this->ID);
}

void VBO::Unbind()
{
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void VBO::Destroy()
{
    glDeleteBuffers(1, &this->ID);
}
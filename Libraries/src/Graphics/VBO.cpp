#include "VBO.h"
#include "Logger.h"


VBO::VBO(VBO&& other) noexcept
{
    std::swap(this->ID, other.ID);
}

VBO& VBO::operator=(VBO&& other) noexcept
{
    if (this != &other) {
        this->Destroy();
        std::swap(this->ID, other.ID);
    }
    return *this;
}

VBO::VBO(std::vector<GLfloat>& vertices) : VBO()
{
    Initialize(vertices);
}

void VBO::Initialize(std::vector<GLfloat>& vertices)
{
    glGenBuffers(1, &this->ID);
    GL_CHECK_ERROR();
    glBindBuffer(GL_ARRAY_BUFFER, this->ID);
    GL_CHECK_ERROR();
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(), GL_STATIC_DRAW);
    GL_CHECK_ERROR();
}

VBO::VBO(std::vector<glm::mat4>& mat4) : VBO()
{
    Initialize(mat4);
}

void VBO::Initialize(std::vector<glm::mat4>& mat4)
{
    glGenBuffers(1, &this->ID);
    GL_CHECK_ERROR();
    glBindBuffer(GL_ARRAY_BUFFER, this->ID);
    GL_CHECK_ERROR();
    glBufferData(GL_ARRAY_BUFFER, mat4.size() * sizeof(glm::mat4), mat4.data(), GL_STATIC_DRAW);
    GL_CHECK_ERROR();
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
    if (this->ID == 0) return;
    glDeleteBuffers(1, &this->ID);
    GL_CHECK_ERROR();
    this->ID = 0;
}

void VBO::UploadData(const void* data, GLsizeiptr size)
{
    this->Bind();
    glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
    GL_CHECK_ERROR();
    this->Unbind();
}
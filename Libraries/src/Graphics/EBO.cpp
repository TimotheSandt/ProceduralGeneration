#include "EBO.h"
#include "Utilities/Logger.h"
#include "utilities.h"

EBO::EBO() : ID(0) {}

EBO::EBO(std::vector<GLuint>& indices) : EBO()
{
    Initialize(indices);
}

void EBO::Initialize(std::vector<GLuint>& indices)
{
    glGenBuffers(1, &this->ID);
    GL_CHECK_ERROR();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->ID);
    GL_CHECK_ERROR();
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);
    GL_CHECK_ERROR();
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
    if (this->ID == 0) return;
    glDeleteBuffers(1, &this->ID);
    GL_CHECK_ERROR_M("Failed to delete EBO");
    this->ID = 0;
    
}

void EBO::UploadData(const void* data, GLsizeiptr size)
{
    this->Bind();
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, size, data);
    GL_CHECK_ERROR();
    this->Unbind();
}

EBO::EBO(EBO&& other) noexcept
    : ID(0)
{
    std::swap(this->ID, other.ID);
}

EBO& EBO::operator=(EBO&& other) noexcept
{
    if (this != &other) {
        this->Destroy();
        std::swap(this->ID, other.ID);
        other.ID = 0;
    }
    return *this;
}
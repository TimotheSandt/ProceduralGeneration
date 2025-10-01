#include "EBO.h"

#include "Logger.h"

EBO::EBO(std::vector<GLuint>& indices) {
    Initialize(indices);
}

EBO::EBO(EBO&& other) noexcept {
    std::swap(this->ID, other.ID);
}

EBO& EBO::operator=(EBO&& other) noexcept {
    if (this != &other) {
        this->Destroy();
        this->Swap(other);
    }
    return *this;
}

void EBO::Swap(EBO& other) noexcept {
    std::swap(this->ID, other.ID);
}

void EBO::Initialize(std::vector<GLuint>& indices) {
    glGenBuffers(1, &this->ID);
    GL_CHECK_ERROR();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->ID);
    GL_CHECK_ERROR();
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);
    GL_CHECK_ERROR();
}

EBO::~EBO() {
    this->Destroy();
}

void EBO::Bind() const {
    if (this->ID == 0) return;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->ID);
}

void EBO::Unbind() const {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void EBO::Destroy() {
    if (this->ID == 0) return;
    glDeleteBuffers(1, &this->ID);
    GL_CHECK_ERROR_M("Failed to delete EBO");
    this->ID = 0;
    
}

void EBO::UploadData(const void* data, GLsizeiptr size) {
    this->Bind();
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, size, data);
    GL_CHECK_ERROR_M("Failed to upload data to EBO");
    this->Unbind();
}
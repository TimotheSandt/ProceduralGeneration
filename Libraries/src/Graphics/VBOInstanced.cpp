#include "VBOInstanced.h"

#include "Logger.h"


VBOInstanced::VBOInstanced(std::vector<glm::mat4>& mat4) : VBO() {
    Initialize(mat4);
}

VBOInstanced::VBOInstanced(VBOInstanced&& other) noexcept {
    this->Swap(other);
}

VBOInstanced& VBOInstanced::operator=(VBOInstanced&& other) noexcept {
    if (this != &other) {
        this->Destroy();
        this->Swap(other);
    }
    return *this;
}

void VBOInstanced::Swap(VBOInstanced& other) noexcept {
    VBO::Swap(other);
    std::swap(this->nbInstances, other.nbInstances);
}

void VBOInstanced::Initialize(std::vector<glm::mat4>& mat4) {
    glGenBuffers(1, &this->ID);
    GL_CHECK_ERROR();
    glBindBuffer(GL_ARRAY_BUFFER, this->ID);
    GL_CHECK_ERROR();
    glBufferData(GL_ARRAY_BUFFER, mat4.size() * sizeof(glm::mat4), mat4.data(), GL_STATIC_DRAW);
    GL_CHECK_ERROR();
}

void VBOInstanced::UploadData(const void* data, GLsizeiptr size) const {
    this->UploadData(data, size, 0);
}

void VBOInstanced::UploadData(const void* data, GLsizeiptr size, size_t offset) const {
    this->Bind();
    glBufferSubData(GL_ARRAY_BUFFER, offset, size, data);
    GL_CHECK_ERROR();
    this->Unbind();
}
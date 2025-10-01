#include "VAO.h"
#include "Logger.h"


VAO::VAO(VAO&& other) noexcept
        : ID(0) {
    std::swap(this->ID, other.ID);
}

VAO& VAO::operator=(VAO&& other) noexcept {
    if (this != &other) {
        this->Destroy();
        std::swap(this->ID, other.ID);
    }
    return *this;
}

void VAO::Initialize() {
    if (this->ID != 0) return;
    glGenVertexArrays(1, &this->ID);
    GL_CHECK_ERROR_M("VAO gen");
}

void VAO::LinkAttrib(VBO& VBO, GLuint layout, GLuint numComponents, GLenum type, GLsizeiptr stride, void* offset) const {
    VBO.Bind();
    glVertexAttribPointer(layout, numComponents, type, GL_FALSE, stride, offset);
    GL_CHECK_ERROR_M("VAO attrib pointer");
    glEnableVertexAttribArray(layout);
    GL_CHECK_ERROR_M("VAO enable attrib");
    VBO.Unbind();
}

void VAO::Bind() const {
    glBindVertexArray(this->ID);
}

void VAO::Unbind() const {
    glBindVertexArray(0);
}

void VAO::Destroy() {
    if (this->ID == 0) return;
    glDeleteVertexArrays(1, &this->ID);
    GL_CHECK_ERROR_M("VAO delete");
    this->ID = 0;
}
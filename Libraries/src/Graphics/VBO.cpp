#include "VBO.h"
#include "Logger.h"


VBO::VBO(VBO&& other) noexcept : ID(0) {
    this->Swap(other);
}

VBO& VBO::operator=(VBO&& other) noexcept {
    if (this != &other) {
        this->Destroy();
        this->Swap(other);
    }
    return *this;
}

VBO::VBO(std::vector<GLfloat>& vertices) {
    Initialize(vertices);
}

VBO::~VBO() {
    this->Destroy();
}

void VBO::Swap(VBO& other) noexcept {
    std::swap(this->ID, other.ID);
}

void VBO::Initialize(std::vector<GLfloat>& vertices) {
    glGenBuffers(1, &this->ID);
    GL_CHECK_ERROR();
    glBindBuffer(GL_ARRAY_BUFFER, this->ID);
    GL_CHECK_ERROR();
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(), GL_STATIC_DRAW);
    GL_CHECK_ERROR();
}

void VBO::Bind() const {
    glBindBuffer(GL_ARRAY_BUFFER, this->ID);
}

void VBO::Unbind() const {
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void VBO::Destroy() {
    if (this->ID == 0) return;
    glDeleteBuffers(1, &this->ID);
    GL_CHECK_ERROR();
    this->ID = 0;
}
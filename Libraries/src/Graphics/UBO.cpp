#include "UBO.h"
#include "Logger.h"

UBO::UBO(size_t size, GLuint bindingPoint, GLenum usage) 
        : bindingPoint(bindingPoint), size(size), usage(usage) {
    initialize(size, bindingPoint);
}

UBO::~UBO() {
    Destroy();
}

UBO::UBO(UBO&& other) noexcept : bindingPoint(0), size(0), usage(GL_DYNAMIC_DRAW) {
    this->Swap(other);
}

UBO& UBO::operator=(UBO&& other) noexcept {
    if (this != &other) {
        this->Destroy();
        this->Swap(other);
    }
    return *this;
}

void UBO::Swap(UBO& other) noexcept {
    std::swap(this->ID, other.ID);
    std::swap(this->bindingPoint, other.bindingPoint);
    std::swap(this->size, other.size);
    std::swap(this->usage, other.usage);
}


bool UBO::initialize(size_t size, GLuint bindingPoint, GLenum usage) {
    this->size = size;
    this->bindingPoint = bindingPoint;
    this->usage = usage;

    glGenBuffers(1, &this->ID);
    GL_CHECK_ERROR_M("UBO gen");
    if (this->ID == 0) return false;

    glBindBuffer(GL_UNIFORM_BUFFER, this->ID);
    GL_CHECK_ERROR_M("UBO bind");
    glBufferData(GL_UNIFORM_BUFFER, this->size, nullptr, this->usage);
    GL_CHECK_ERROR_M("UBO buffer data");
    glBindBufferBase(GL_UNIFORM_BUFFER, this->bindingPoint, this->ID);
    GL_CHECK_ERROR_M("UBO bind base");
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    GL_CHECK_ERROR_M("UBO unbind");

    return true;
}

void UBO::Destroy() {
    if (this->ID == 0) return;
    glDeleteBuffers(1, &this->ID);
    GL_CHECK_ERROR_M("UBO delete");
    this->ID = 0;
    this->bindingPoint = 0;
    this->size = 0;
}

void UBO::Bind() const {
    if (this->ID == 0) return;
    glBindBuffer(GL_UNIFORM_BUFFER, this->ID);
}

void UBO::BindToBindingPoint() const {
    if (this->ID == 0) return;
    glBindBufferBase(GL_UNIFORM_BUFFER, this->bindingPoint, this->ID);
    GL_CHECK_ERROR_M("UBO bind to point");
}

void UBO::Unbind() const {
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void UBO::uploadData(const void* data, size_t size, size_t offset) const {
    if (this->ID == 0) return;
    glBindBuffer(GL_UNIFORM_BUFFER, this->ID);
    GL_CHECK_ERROR_M("UBO upload bind");
    glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
    GL_CHECK_ERROR_M("UBO upload subdata");
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    GL_CHECK_ERROR_M("UBO upload unbind");
}


void* UBO::mapBuffer(GLenum access) const {
    if (this->ID == 0) return nullptr;
    glBindBuffer(GL_UNIFORM_BUFFER, this->ID);
    return glMapBufferRange(GL_UNIFORM_BUFFER, 0, this->size, access);
}

void UBO::unmapBuffer() const {
    if (this->ID == 0) return;
    glUnmapBuffer(GL_UNIFORM_BUFFER);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}


#include "SSBO.h"

SSBO::SSBO() : ID(0), bindingPoint(0), size(0), usage(DYNAMIC_DRAW) {}

SSBO::SSBO(size_t size, GLuint bindingPoint, Usage usage) : ID(0), bindingPoint(bindingPoint), size(size), usage(usage)
{
    initialize(size, bindingPoint);
}

SSBO::~SSBO()
{
    destroy();
}


SSBO::SSBO(SSBO&& other) noexcept : ID(other.ID), bindingPoint(other.bindingPoint), size(other.size), usage(other.usage) {
    other.ID = 0;
    other.bindingPoint = 0;
    other.size = 0;
    other.usage = DYNAMIC_DRAW;
}

SSBO& SSBO::operator=(SSBO&& other) noexcept
{
    if (this == &other) return *this;
    destroy();
    ID = other.ID;
    bindingPoint = other.bindingPoint;
    size = other.size;
    usage = other.usage;

    other.ID = 0;
    other.bindingPoint = 0;
    other.size = 0;
    other.usage = DYNAMIC_DRAW;
    return *this;
}

bool SSBO::initialize(size_t size, GLuint bindingPoint, Usage usage)
{
    this->size = size;
    this->bindingPoint = bindingPoint;
    this->usage = usage;

    glGenBuffers(1, &this->ID);
    if (this->ID == 0) return false;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->ID);
    glBufferData(GL_SHADER_STORAGE_BUFFER, this->size, nullptr, static_cast<GLenum>(usage));
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, this->bindingPoint, this->ID);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        LOG_ERROR(error, "Error initializing SSBO");
        return false;
    }

    return true;
}

void SSBO::destroy()
{
    if (this->ID != 0) glDeleteBuffers(1, &this->ID);
    this->ID = 0;
    this->bindingPoint = 0;
    this->size = 0;
}

void SSBO::bind() const
{
    if (this->ID == 0) return;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->ID);
}

void SSBO::bindToPoint() const
{
    if (this->ID == 0) return;
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, this->bindingPoint, this->ID);
#ifdef DEBUG
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) LOG_ERROR(error, "Error binding SSBO to binding point");
#endif
}

void SSBO::unbind() const
{
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void SSBO::uploadData(const void* data, size_t size, size_t offset)
{
    if (this->ID == 0) return;
    ensureCapacity(offset + size);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->ID);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, data);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
#ifdef DEBUG
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) LOG_ERROR(error, "Error uploading data to SSBO");
#endif
}

void SSBO::downloadData(void* data, size_t size, size_t offset) const
{
    if (this->ID == 0) return;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->ID);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, data);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
#ifdef DEBUG
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) LOG_ERROR(error, "Error downloading data from SSBO");
#endif
}



void SSBO::resize(size_t newSize) { 
    this->size = newSize; 

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->ID);
    glBufferData(GL_SHADER_STORAGE_BUFFER, this->size, nullptr, static_cast<GLenum>(this->usage));
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void SSBO::resizePreserveData(size_t newSize) {
    GLuint oldID = this->ID;
    size_t oldSize = this->size;

    this->size = newSize;

    glGenBuffers(1, &this->ID);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->ID);
    glBufferData(GL_SHADER_STORAGE_BUFFER, this->size, nullptr, static_cast<GLenum>(this->usage));
    
    size_t copySize = std::min(oldSize, this->size);
    glBindBuffer(GL_COPY_READ_BUFFER, oldID);
    glBindBuffer(GL_COPY_WRITE_BUFFER, this->ID);
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, copySize);
    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    glDeleteBuffers(1, &oldID);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, this->bindingPoint, this->ID);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}



void* SSBO::mapBuffer(GLenum access)
{
    if (this->ID == 0) return nullptr;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->ID);
    return glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, this->size, access);
}

void SSBO::unmapBuffer()
{
    if (this->ID == 0) return;
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void SSBO::ensureCapacity(size_t newSize) {
    if (newSize > this->size) {
        this->resizePreserveData(newSize);
    }
}
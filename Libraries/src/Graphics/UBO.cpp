#include "UBO.h"

UBO::UBO(size_t size, GLuint bindingPoint, GLenum usage) : bindingPoint(bindingPoint), size(size), usage(usage)
{
    initialize(size, bindingPoint);
}

UBO::~UBO()
{
    Destroy();
}

bool UBO::initialize(size_t size, GLuint bindingPoint, GLenum usage)
{
    this->size = size;
    this->bindingPoint = bindingPoint;
    this->usage = usage;

    glGenBuffers(1, &this->ID);
    if (this->ID == 0) return false;

    glBindBuffer(GL_UNIFORM_BUFFER, this->ID);
    glBufferData(GL_UNIFORM_BUFFER, this->size, nullptr, this->usage);
    glBindBufferBase(GL_UNIFORM_BUFFER, this->bindingPoint, this->ID);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        LOG_ERROR(error, "Error initializing UBO");
        return false;
    }

    return true;
}

void UBO::Destroy()
{
    if (this->ID != 0) glDeleteBuffers(1, &this->ID);
    this->ID = 0;
    this->bindingPoint = 0;
    this->size = 0;
}

void UBO::Bind()
{
    if (this->ID == 0) return;
    glBindBuffer(GL_UNIFORM_BUFFER, this->ID);
}

void UBO::BindToBindingPoint()
{
    if (this->ID == 0) return;
    glBindBufferBase(GL_UNIFORM_BUFFER, this->bindingPoint, this->ID);
#ifdef DEBUG
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) 
        LOG_ERROR(error, "Error binding UBO to binding point");
#endif
}

void UBO::Unbind()
{
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void UBO::uploadData(const void* data, size_t size, size_t offset)
{
    if (this->ID == 0) return;
    glBindBuffer(GL_UNIFORM_BUFFER, this->ID);
    glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
#ifdef DEBUG
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) LOG_ERROR(error, "Error uploading data to UBO");
#endif
}


void* UBO::mapBuffer(GLenum access)
{
    if (this->ID == 0) return nullptr;
    glBindBuffer(GL_UNIFORM_BUFFER, this->ID);
    return glMapBufferRange(GL_UNIFORM_BUFFER, 0, this->size, access);
}

void UBO::unmapBuffer()
{
    if (this->ID == 0) return;
    glUnmapBuffer(GL_UNIFORM_BUFFER);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}


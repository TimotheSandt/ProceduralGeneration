#include "UBO.h"

#include <iostream>

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
        std::cout << "Error initializing UBO: " << error << std::endl;
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
        std::cout << "Error binding UBO to binding point: " << error << std::endl;
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
    if (error != GL_NO_ERROR) std::cout << "Error uploading data to UBO: " << error << std::endl;
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


#include "SSBO.h"
#include "Logger.h"

SSBO::SSBO() : ID(0), bindingPoint(0), size(0), usage(DYNAMIC_DRAW) {}

SSBO::SSBO(size_t size, GLuint bindingPoint, Usage usage) : ID(0), bindingPoint(bindingPoint), size(size), usage(usage)
{
    Initialize(size, bindingPoint);
}

SSBO::~SSBO()
{
    Destroy();
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
    Destroy();
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

bool SSBO::Initialize(size_t size, GLuint bindingPoint, Usage usage)
{
    this->size = size;
    this->bindingPoint = bindingPoint;
    this->usage = usage;

    glGenBuffers(1, &this->ID);
    GL_CHECK_ERROR();
    if (this->ID == 0) return false;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->ID);
    GL_CHECK_ERROR();
    glBufferData(GL_SHADER_STORAGE_BUFFER, this->size, nullptr, static_cast<GLenum>(usage));
    GL_CHECK_ERROR();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, this->bindingPoint, this->ID);
    GL_CHECK_ERROR();
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    GL_CHECK_ERROR_M("Failed to initialize SSBO");

    return true;
}

void SSBO::Destroy()
{
    if (this->ID == 0) return
    glDeleteBuffers(1, &this->ID);
    GL_CHECK_ERROR();
    this->ID = 0;
    this->bindingPoint = 0;
    this->size = 0;
}

void SSBO::Bind() const
{
    if (this->ID == 0) return;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->ID);
}

void SSBO::BindToPoint() const
{
    if (this->ID == 0) return;
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, this->bindingPoint, this->ID);
    GL_CHECK_ERROR_M("Failed to bind SSBO");
}

void SSBO::Unbind() const
{
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void SSBO::UploadData(const void* data, size_t size, size_t offset)
{
    if (this->ID == 0) return;
    ensureCapacity(offset + size);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->ID);
    GL_CHECK_ERROR();
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, data);
    GL_CHECK_ERROR();
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    GL_CHECK_ERROR_M("Error uploading data to SSBO");
}

void SSBO::DownloadData(void* data, size_t size, size_t offset) const
{
    if (this->ID == 0) return;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->ID);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, data);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
#ifdef DEBUG
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) LOG_ERROR(error, "Error downloading data from SSBO");
#endif
}



void SSBO::Resize(size_t newSize) {
    this->size = newSize;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->ID);
    GL_CHECK_ERROR();
    glBufferData(GL_SHADER_STORAGE_BUFFER, this->size, nullptr, static_cast<GLenum>(this->usage));
    GL_CHECK_ERROR();
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    GL_CHECK_ERROR();
}

void SSBO::ResizePreserveData(size_t newSize) {
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



void* SSBO::MapBuffer(GLenum access)
{
    if (this->ID == 0) return nullptr;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->ID);
    GL_CHECK_ERROR();
    void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, this->size, access);
    GL_CHECK_ERROR();
    return ptr;
}

void SSBO::UnmapBuffer()
{
    if (this->ID == 0) return;
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    GL_CHECK_ERROR();
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    GL_CHECK_ERROR();
}

void SSBO::ensureCapacity(size_t newSize) {
    if (newSize > this->size) {
        this->ResizePreserveData(newSize);
    }
}
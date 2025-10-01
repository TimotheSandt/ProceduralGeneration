#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Logger.h"

#include <stdexcept>
#include <vector>
#include <cstring>


class SSBO
{
public:
    
    enum Usage {
        STATIC_DRAW = GL_STATIC_DRAW,
        DYNAMIC_DRAW = GL_DYNAMIC_DRAW,
        STREAM_DRAW = GL_STREAM_DRAW
    };

    SSBO();
    SSBO(size_t size, GLuint bindingPoint, Usage usage = DYNAMIC_DRAW);
    ~SSBO();

    SSBO(const SSBO&) = delete;
    SSBO& operator=(const SSBO&) = delete;

    SSBO(SSBO&& other) noexcept;
    SSBO& operator=(SSBO&& other) noexcept;

    bool Initialize(size_t size, GLuint bindingPoint, Usage usage = DYNAMIC_DRAW);

    void UploadData(const void* data, size_t size, size_t offset = 0);
    void DownloadData(void* data, size_t size, size_t offset = 0) const;

    void Resize(size_t newSize);
    void ResizePreserveData(size_t newSize);

    void Bind() const;
    void BindToPoint() const;
    void Unbind() const;

    void Clear();

    void Destroy();

    bool isInitialized() const { return ID != 0; }

    GLuint getBufferID() const { return ID; }
    size_t getSize() const { return size; }
    GLuint getBindingPoint() const { return bindingPoint; }
    Usage getUsage() const { return usage; }

    void* MapBuffer(GLenum access = GL_READ_WRITE);
    void UnmapBuffer();

    static void checkGLError(const std::string& operation);

private:
    void Swap(SSBO& other) noexcept;
    void ensureCapacity(size_t requiredSize);

private:
    GLuint ID = 0;
    GLuint bindingPoint;
    size_t size;
    Usage usage;

};
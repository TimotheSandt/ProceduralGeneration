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

    bool initialize(size_t size, GLuint bindingPoint, Usage usage = DYNAMIC_DRAW);

    void uploadData(const void* data, size_t size, size_t offset = 0);
    void downloadData(void* data, size_t size, size_t offset = 0) const;

    void resize(size_t newSize);
    void resizePreserveData(size_t newSize);

    void bind() const;
    void bindToPoint() const;
    void unbind() const;

    void clear();

    void destroy();

    bool isInitialized() const { return ID != 0; }

    GLuint getBufferID() const { return ID; }
    size_t getSize() const { return size; }
    GLuint getBindingPoint() const { return bindingPoint; }
    Usage getUsage() const { return usage; }

    void* mapBuffer(GLenum access = GL_READ_WRITE);
    void unmapBuffer();

    static void checkGLError(const std::string& operation);

private:
    GLuint ID;
    GLuint bindingPoint;
    size_t size;
    Usage usage;

    void ensureCapacity(size_t requiredSize);
};
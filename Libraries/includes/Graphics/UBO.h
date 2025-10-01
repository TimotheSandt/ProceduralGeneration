#pragma once


#define CAMERA_BINDING_POINT 0
#define LIGHT_BINDING_POINT 1
#define SKYBOX_BINDING_POINT 2
#define MESH_MODEL_BINDING_POINT 3


#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Logger.h"

#include <vector>


class UBO {
public:
    UBO() = default;
    UBO(size_t size, GLuint bindingPoint, GLenum usage = GL_DYNAMIC_DRAW);
    ~UBO();

    UBO(const UBO&) = delete;
    UBO& operator=(const UBO&) = delete;

    UBO(UBO&&) noexcept;
    UBO& operator=(UBO&&) noexcept;

    bool initialize(size_t size, GLuint bindingPoint, GLenum usage = GL_DYNAMIC_DRAW);
    void Destroy();

    void Bind();
    void BindToBindingPoint();
    void Unbind();

    void uploadData(const void* data, size_t size, size_t offset = 0);

    /*
    access :
        GL_READ_ONLY - lecture seule
        GL_WRITE_ONLY - écriture seule
        GL_READ_WRITE - lecture/écriture
    */
    void* mapBuffer(GLenum access = GL_READ_WRITE);
    void unmapBuffer();

private:
    void Swap(UBO& other) noexcept;


private:
    GLuint ID = 0;
    GLuint bindingPoint;
    size_t size;
    GLenum usage;
};
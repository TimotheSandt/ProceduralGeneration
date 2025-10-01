#pragma once

#include <glad/glad.h>
#include "VBO.h"

class VAO
{
public:
    VAO() {};
    ~VAO() { this->Destroy(); };

    VAO(const VAO&) = delete;
    VAO& operator=(const VAO&) = delete;

    VAO(VAO&& other) noexcept;
    VAO& operator=(VAO&& other) noexcept;

    void Initialize();


	void LinkAttrib(VBO& VBO, GLuint layout, GLuint numComponents, GLenum type, GLsizeiptr stride, void* offset) const;
    void Bind() const;
    void Unbind() const;
    void Destroy();

private:
    GLuint ID = 0;
};
#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>
#include <vector>

class VBO
{
public:
    VBO() = default;
    VBO(std::vector<GLfloat>& vertices);
    ~VBO();

    VBO(const VBO&) = delete;
    VBO& operator=(const VBO&) = delete;

    VBO(VBO&&) noexcept;
    VBO& operator=(VBO&&) noexcept;

    void Initialize(std::vector<GLfloat>& vertices);

    void Bind() const;
    void Unbind() const;
    void Destroy();

protected:
    void Swap(VBO& other) noexcept;

protected:
    GLuint ID = 0;    
};
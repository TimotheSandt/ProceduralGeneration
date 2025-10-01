#pragma once

#include <glad/glad.h>
#include <vector>

class EBO
{
public:
    EBO() = default;
    EBO(std::vector<GLuint>& indices);
    ~EBO();

    EBO(const EBO&) = delete;
    EBO& operator=(const EBO&) = delete;

    EBO(EBO&&) noexcept;
    EBO& operator=(EBO&&) noexcept;

    void Swap(EBO& other) noexcept;

    void Initialize(std::vector<GLuint>& indices);

    void Bind() const;
    void Unbind() const;
    void Destroy();
    void UploadData(const void* data, GLsizeiptr size);

private:
    GLuint ID = 0;
};
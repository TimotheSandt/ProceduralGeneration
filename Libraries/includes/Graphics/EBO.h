#pragma once

#include <glad/glad.h>
#include <vector>

class EBO
{
public:
    EBO();
    EBO(std::vector<GLuint>& indices);
    ~EBO();

    EBO(const EBO&) = delete;
    EBO& operator=(const EBO&) = delete;

    EBO(EBO&& other) noexcept;
    EBO& operator=(EBO&& other) noexcept;

    void Initialize(std::vector<GLuint>& indices);

    void Bind();
    void Unbind();
    void Destroy();
    void UploadData(const void* data, GLsizeiptr size);

private:
    GLuint ID;
};
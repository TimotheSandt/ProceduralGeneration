#pragma once

#include <glad/glad.h>

#include "Graphics/Core/GraphicsResources.h"
#include "Logger.h"

#include <cerrno>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

std::string get_file_contents(const char *filename);

class Shader
{
  public:
    Shader() = default;
    Shader(const char *vertexPath, const char *fragmentPath);
    ~Shader();

    Shader(const Shader &shader);
    Shader &operator=(const Shader &);

    Shader(Shader &&) noexcept;
    Shader &operator=(Shader &&) noexcept;

    void SetShader(const char *vertexPath, const char *fragmentPath);
    void SetShaderCode(std::string vertexCode, std::string fragmentCode);
    void CompileShader();

    void Bind() const;
    void Unbind() const;
    void Destroy();
    GLint GetUniformLocation(const std::string &uniform) const;
    void SetUniformFloats(GLint location, const GLfloat *data, std::size_t componentCount) const;
    void SetUniformInts(GLint location, const GLint *data, std::size_t componentCount) const;
    void SetUniformMatrix4(GLint location, const GLfloat *data) const;

    GLuint GetID() const { return this->ID; }
    bool IsCompiled() const { return this->ID != 0; }

  private:
    GLuint ID = 0;
    std::unique_ptr<IShaderProgramResource> backendResource;

    const char *vertexShaderPath;
    const char *fragmentShaderPath;
    std::string vertexSource;
    std::string fragmentSource;

  private:
    void Swap(Shader &other) noexcept;
};

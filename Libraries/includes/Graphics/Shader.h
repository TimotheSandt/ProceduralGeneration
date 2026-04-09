#pragma once

#include "Graphics/Core/GraphicsResources.h"
#include "Logger.h"

#include <cerrno>
#include <cstdint>
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
    int GetUniformLocation(const std::string &uniform) const;
    void SetUniformFloats(int location, const float *data, std::size_t componentCount) const;
    void SetUniformInts(int location, const int *data, std::size_t componentCount) const;
    void SetUniformMatrix4(int location, const float *data) const;

    std::uint32_t GetID() const { return this->ID; }
    bool IsCompiled() const { return this->ID != 0; }

  private:
    std::uint32_t ID = 0;
    std::unique_ptr<IShaderProgramResource> backendResource;

    const char *vertexShaderPath;
    const char *fragmentShaderPath;
    std::string vertexSource;
    std::string fragmentSource;

  private:
    void Swap(Shader &other) noexcept;
};

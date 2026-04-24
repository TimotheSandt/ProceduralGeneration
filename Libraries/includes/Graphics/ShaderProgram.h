#pragma once

#include "Graphics/Core/GraphicsResources.h"
#include "Logger.h"

#include <cerrno>
#include <cstdint>
#include <fstream>
#include <iosfwd>
#include <memory>
#include <sstream>
#include <string>

std::string get_file_contents(const char *filename);

class ShaderProgram
{
  public:
    ShaderProgram() = default;
    ShaderProgram(const char *vertexPath, const char *fragmentPath);
    ~ShaderProgram();

    ShaderProgram(const ShaderProgram &shaderProgram);
    ShaderProgram &operator=(const ShaderProgram &);

    ShaderProgram(ShaderProgram &&) noexcept;
    ShaderProgram &operator=(ShaderProgram &&) noexcept;

    void SetShader(const char *vertexPath, const char *fragmentPath);
    void SetShaderCode(std::string vertexCode, std::string fragmentCode);
    void CompileShader();

    // Binary cache — used by ShaderLibrary. Returns false if the backend does not
    // support binary programs (non-OpenGL) or if no program is compiled yet.
    bool SaveBinary(std::ostream &out) const;
    bool LoadBinary(std::istream &in);

    void Bind() const;
    void Unbind() const;
    void Destroy();
    int GetUniformLocation(const std::string &uniform) const;
    void SetUniformFloats(int location, const float *data, std::size_t componentCount) const;
    void SetUniformInts(int location, const int *data, std::size_t componentCount) const;
    void SetUniformMatrix4(int location, const float *data) const;

    std::uint32_t GetID() const { return this->ID; }
    bool IsCompiled() const { return this->backendResource != nullptr; }

  private:
    std::uint32_t ID = 0;
    // shared_ptr so multiple ShaderProgram handles (e.g. from ShaderLibrary) can
    // refer to the same compiled GPU program without recompiling or copying.
    std::shared_ptr<IShaderProgramResource> backendResource;

    const char *vertexShaderPath;
    const char *fragmentShaderPath;
    std::string vertexSource;
    std::string fragmentSource;

  private:
    void Swap(ShaderProgram &other) noexcept;
};

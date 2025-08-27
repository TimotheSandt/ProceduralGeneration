#pragma once

#include <glad/glad.h>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cerrno>

std::string get_file_contents(const char* filename);

class Shader
{
public:
    Shader(const char* vertexPath, const char* fragmentPath);
    Shader() = default;

    void SetShader(const char* vertexPath, const char* fragmentPath);
    void SetShaderCode(std::string vertexCode, std::string fragmentCode);
    void SetShaderCode(const char* vertexCode, const char* fragmentCode);
    void CompileShader();

    void Bind();
    void Unbind();
    void Destroy();

    GLuint GetID() { return this->ID; }

private:
    GLuint ID;

    const char* vertexShaderPath;
    const char* fragmentShaderPath;
    const char* vertexSource;
    const char* fragmentSource;

    bool isCompiled = false;

private:
	void compileErrors(unsigned int shader, const char* type);
};
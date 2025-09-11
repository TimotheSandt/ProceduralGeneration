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
    void CompileShader();

    void Bind();
    void Unbind();
    void Destroy();

    GLuint GetID() { return this->ID; }
    bool IsCompiled() { return this->ID != 0; }

private:
    GLuint ID = 0;

    const char* vertexShaderPath;
    const char* fragmentShaderPath;
    std::string vertexSource;
    std::string fragmentSource;

private:
	bool compileErrors(unsigned int shader, const char* type);

    static GLuint& CurrentBoundShader() {
        static GLuint currentBoundShader = 0;
        return currentBoundShader;
    } ;
};
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
    void CompileShader();

    void Activate();
    void Destroy();

    GLuint GetID() { return this->ID; }

private:
    GLuint ID;

    const char* vertexShaderPath;
    const char* fragmentShaderPath;

    bool isCompiled = false;

private:
	void compileErrors(unsigned int shader, const char* type);
};
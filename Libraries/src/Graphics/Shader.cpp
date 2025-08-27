#include "Shader.h"
#include <cstring>

// Reads a text file and outputs a string with everything in the text file
std::string get_file_contents(const char* filename)
{
	std::ifstream in(filename, std::ios::binary);
	if (in)
	{
		std::string contents;
		in.seekg(0, std::ios::end);
		contents.resize(in.tellg());
		in.seekg(0, std::ios::beg);
		in.read(&contents[0], contents.size());
		in.close();
		return(contents);
	}
	throw(errno);
}

Shader::Shader(const char* vertexFile, const char* fragmentFile)
	: vertexShaderPath(vertexFile), fragmentShaderPath(fragmentFile)
{
	this->CompileShader();
}

void Shader::SetShader(const char* vertexPath, const char* fragmentPath)
{
	this->vertexShaderPath = vertexPath;
	this->fragmentShaderPath = fragmentPath;
	this->CompileShader();
}

void Shader::CompileShader()
{
	if (this->isCompiled)
		this->Destroy();
	std::string vertexCode = get_file_contents(this->vertexShaderPath);
    std::string fragmentCode = get_file_contents(this->fragmentShaderPath);

    const char* vertexSource = vertexCode.c_str();
    const char* fragmentSource = fragmentCode.c_str();

    // Build and compile the vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);
    this->compileErrors(vertexShader, "VERTEX");

    // Build and compile the fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);
    this->compileErrors(fragmentShader, "FRAGMENT");
    
    // Link the vertex and fragment shader into a shader program
    this->ID = glCreateProgram();
    glAttachShader(this->ID, vertexShader);
    glAttachShader(this->ID, fragmentShader);
    glLinkProgram(this->ID);
    this->compileErrors(this->ID, "PROGRAM");

    // Delete the shaders as they're linked into our program now and no longer necessary
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

	this->isCompiled = true;
}

void Shader::Bind()
{
	if (this->isCompiled)
    	glUseProgram(this->ID);
	else 
		std::cout << "Shader not compiled" << std::endl;
}

void Shader::Unbind()
{
	glUseProgram(0);
}

void Shader::Destroy()
{
    if (this->ID != 0) glDeleteProgram(this->ID);
	this->ID = 0;
	this->isCompiled = false;
}


// Checks if the different Shaders have compiled properly
void Shader::compileErrors(unsigned int shader, const char* type)
{
	// Stores status of compilation
	GLint hasCompiled;
	// Character array to store error message in
	char infoLog[1024];
	if (strcmp(type, "PROGRAM") != 0)
	{
		glGetShaderiv(shader, GL_COMPILE_STATUS, &hasCompiled);
		if (hasCompiled == GL_FALSE)
		{
			glGetShaderInfoLog(shader, 1024, NULL, infoLog);
			std::cout << "SHADER_COMPILATION_ERROR for:" << type << "\n" << infoLog << std::endl;
		}
	}
	else
	{
		glGetProgramiv(shader, GL_LINK_STATUS, &hasCompiled);
		if (hasCompiled == GL_FALSE)
		{
			glGetProgramInfoLog(shader, 1024, NULL, infoLog);
			std::cout << "SHADER_LINKING_ERROR for:" << type << "\n" << infoLog << std::endl;
		}
	}
}
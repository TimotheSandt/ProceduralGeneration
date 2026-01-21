#include "Shader.h"
#include <cstring>

#define COMPILE_SUCCESS 0
#define COMPILE_ERRORS 1


// Reads a text file and outputs a string with everything in the text file
std::string get_file_contents(const char* filename) {
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
		: vertexShaderPath(vertexFile), fragmentShaderPath(fragmentFile) {
	this->SetShader(vertexFile, fragmentFile);
	this->CompileShader();
}

Shader::~Shader() {
	this->Destroy();
}

Shader::Shader(const Shader& shader) noexcept {
	this->SetShader(shader.vertexShaderPath, shader.fragmentShaderPath);
	this->CompileShader();
}

Shader& Shader::operator=(const Shader& shader) noexcept {
	if (this != &shader) {
		this->SetShader(shader.vertexShaderPath, shader.fragmentShaderPath);
		this->CompileShader();
	}
	return *this;
}


Shader::Shader(Shader&& shader) noexcept : ID(0), vertexShaderPath(nullptr), fragmentShaderPath(nullptr) {
	this->Swap(shader);
}

Shader& Shader::operator=(Shader&& shader) noexcept {
	if (this != &shader) {
		this->Destroy();
		this->Swap(shader);
	}
	return *this;
}

void Shader::Swap(Shader& other) noexcept {
	std::swap(this->ID, other.ID);
	std::swap(this->vertexShaderPath, other.vertexShaderPath);
	std::swap(this->fragmentShaderPath, other.fragmentShaderPath);
	std::swap(this->vertexSource, other.vertexSource);
	std::swap(this->fragmentSource, other.fragmentSource);
}


void Shader::SetShader(const char* vertexPath, const char* fragmentPath) {
	this->vertexShaderPath = vertexPath;
	this->fragmentShaderPath = fragmentPath;

	try {
	    vertexSource = get_file_contents(this->vertexShaderPath);
	} catch (const std::exception& e) {
	    LOG_ERROR(1, "Failed to load vertex shader from ", this->vertexShaderPath, ": ", e.what());
	    vertexSource = "#version 330 core\nvoid main() { gl_Position = vec4(0.0); }";
	} catch (int e) {
		LOG_ERROR(1, "Failed to load vertex shader from ", this->vertexShaderPath, ": ", strerror(e));
		vertexSource = "#version 330 core\nvoid main() { gl_Position = vec4(0.0); }";
	}

	try {
	    fragmentSource = get_file_contents(this->fragmentShaderPath);
	} catch (const std::exception& e) {
	    LOG_ERROR(1, "Failed to load fragment shader from ", this->fragmentShaderPath, ": ", e.what());
	    fragmentSource = "#version 330 core\nout vec4 FragColor; void main() { FragColor = vec4(1.0); }";
	} catch (int e) {
		LOG_ERROR(1, "Failed to load fragment shader from ", this->fragmentShaderPath, ": ", strerror(e));
		fragmentSource = "#version 330 core\nout vec4 FragColor; void main() { FragColor = vec4(1.0); }";
	}

	this->CompileShader();
}

void Shader::SetShaderCode(std::string vertexCode, std::string fragmentCode) {
	this->vertexShaderPath = nullptr;
	this->fragmentShaderPath = nullptr;


	this->vertexSource = vertexCode;
	this->fragmentSource = fragmentCode;

	this->CompileShader();
}

void Shader::CompileShader() {
	this->Destroy();

	const char* vSource = this->vertexSource.c_str();
	const char* fSource = this->fragmentSource.c_str();


    // Build and compile the vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vSource, NULL);
    glCompileShader(vertexShader);
    this->compileErrors(vertexShader, "VERTEX");

    // Build and compile the fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fSource, NULL);
    glCompileShader(fragmentShader);
    this->compileErrors(fragmentShader, "FRAGMENT");

    // Link the vertex and fragment shader into a shader program
    this->ID = glCreateProgram();
    glAttachShader(this->ID, vertexShader);
    glAttachShader(this->ID, fragmentShader);
    glLinkProgram(this->ID);
    if (this->compileErrors(this->ID, "PROGRAM") == COMPILE_ERRORS) {
		this->Destroy();
	}

    // Delete the shaders as they're linked into our program now and no longer necessary
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void Shader::Bind() const {
	if (this->ID == 0) return;
	glUseProgram(this->ID);
}

void Shader::Unbind() const {
	glUseProgram(0);
}

void Shader::Destroy() {
    if (this->ID == 0)  return;
	glDeleteProgram(this->ID);
	this->ID = 0;
}


// Checks if the different Shaders have compiled properly
bool Shader::compileErrors(unsigned int shader, const char* type) const {
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
			LOG_ERROR(1, "SHADER_COMPILATION_ERROR for:", type, "\n", infoLog);
		}
	}
	else
	{
		glGetProgramiv(shader, GL_LINK_STATUS, &hasCompiled);
		if (hasCompiled == GL_FALSE)
		{
			glGetProgramInfoLog(shader, 1024, NULL, infoLog);
			LOG_ERROR(1, "SHADER_LINKING_ERROR for:", type, "\n", infoLog);
		}
	}
	return (hasCompiled) ? COMPILE_SUCCESS : COMPILE_ERRORS;
}

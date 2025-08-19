#include "Mesh.h"

void Mesh::InitUniform4f(const char* uniform, const GLfloat* data)
{
    this->shader.Activate();
    glUniform4fv(glGetUniformLocation(this->shader.GetID(), uniform), 1, data);
}

void Mesh::InitUniform3f(const char* uniform, const GLfloat* data)
{
    this->shader.Activate();
    glUniform3fv(glGetUniformLocation(this->shader.GetID(), uniform), 1, data);
}

void Mesh::InitUniform2f(const char* uniform, const GLfloat* data)
{
    this->shader.Activate();
    glUniform2fv(glGetUniformLocation(this->shader.GetID(), uniform), 1, data);
}

void Mesh::InitUniform1f(const char* uniform, const GLfloat* data)
{
    this->shader.Activate();
    glUniform1fv(glGetUniformLocation(this->shader.GetID(), uniform), 1, data);
}

void Mesh::InitUniform4i(const char* uniform, const GLint* data)
{
    this->shader.Activate();
    glUniform4iv(glGetUniformLocation(this->shader.GetID(), uniform), 1, data);
}

void Mesh::InitUniform3i(const char* uniform, const GLint* data)
{
    this->shader.Activate();
    glUniform3iv(glGetUniformLocation(this->shader.GetID(), uniform), 1, data);
}

void Mesh::InitUniform2i(const char* uniform, const GLint* data)
{
    this->shader.Activate();
    glUniform2iv(glGetUniformLocation(this->shader.GetID(), uniform), 1, data);
}

void Mesh::InitUniform1i(const char* uniform, const GLint* data)
{
    this->shader.Activate();
    glUniform1iv(glGetUniformLocation(this->shader.GetID(), uniform), 1, data);
}

void Mesh::InitUniformMatrix4f(const char* uniform, const GLfloat* data)
{
    this->shader.Activate();
    glUniformMatrix4fv(glGetUniformLocation(this->shader.GetID(), uniform), 1, GL_FALSE, data);
}
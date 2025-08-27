#include "Mesh.h"

void Mesh::InitUniform4f(const char* uniform, const GLfloat* data)
{
    this->shader.Bind();
    glUniform4fv(glGetUniformLocation(this->shader.GetID(), uniform), 1, data);
}

void Mesh::InitUniform3f(const char* uniform, const GLfloat* data)
{
    this->shader.Bind();
    glUniform3fv(glGetUniformLocation(this->shader.GetID(), uniform), 1, data);
}

void Mesh::InitUniform2f(const char* uniform, const GLfloat* data)
{
    this->shader.Bind();
    glUniform2fv(glGetUniformLocation(this->shader.GetID(), uniform), 1, data);
}

void Mesh::InitUniform1f(const char* uniform, const GLfloat* data)
{
    this->shader.Bind();
    glUniform1fv(glGetUniformLocation(this->shader.GetID(), uniform), 1, data);
}

void Mesh::InitUniform4i(const char* uniform, const GLint* data)
{
    this->shader.Bind();
    glUniform4iv(glGetUniformLocation(this->shader.GetID(), uniform), 1, data);
}

void Mesh::InitUniform3i(const char* uniform, const GLint* data)
{
    this->shader.Bind();
    glUniform3iv(glGetUniformLocation(this->shader.GetID(), uniform), 1, data);
}

void Mesh::InitUniform2i(const char* uniform, const GLint* data)
{
    this->shader.Bind();
    glUniform2iv(glGetUniformLocation(this->shader.GetID(), uniform), 1, data);
}

void Mesh::InitUniform1i(const char* uniform, const GLint* data)
{
    this->shader.Bind();
    glUniform1iv(glGetUniformLocation(this->shader.GetID(), uniform), 1, data);
}

void Mesh::InitUniformMatrix4f(const char* uniform, const GLfloat* data)
{
    this->shader.Bind();
    glUniformMatrix4fv(glGetUniformLocation(this->shader.GetID(), uniform), 1, GL_FALSE, data);
}
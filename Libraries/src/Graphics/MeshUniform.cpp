#include "Mesh.h"

void Mesh::InitUniform4f(const char* uniform, const GLfloat* data) {
    std::string sUni(uniform);
    if (!CacheUniform(sUni, (void*)data, 4 * sizeof(GLfloat))) return;
    this->shader.Bind();
    glUniform4fv(CachedUniformLocation(sUni), 1, data);
}

void Mesh::InitUniform3f(const char* uniform, const GLfloat* data) {
    std::string sUni(uniform);
    if (!CacheUniform(sUni, (void*)data, 3 * sizeof(GLfloat))) return;
    this->shader.Bind();
    glUniform3fv(CachedUniformLocation(sUni), 1, data);
}

void Mesh::InitUniform2f(const char* uniform, const GLfloat* data) {
    std::string sUni(uniform);
    if (!CacheUniform(sUni, (void*)data, 2 * sizeof(GLfloat))) return;
    this->shader.Bind();
    glUniform2fv(CachedUniformLocation(sUni), 1, data);
}

void Mesh::InitUniform1f(const char* uniform, const GLfloat* data) {
    std::string sUni(uniform);
    if (!CacheUniform(sUni, (void*)data, sizeof(GLfloat))) return;
    this->shader.Bind();
    glUniform1fv(CachedUniformLocation(sUni), 1, data);
}

void Mesh::InitUniform4i(const char* uniform, const GLint* data) {
    std::string sUni(uniform);
    if (!CacheUniform(sUni, (void*)data, 4 * sizeof(GLint))) return;
    this->shader.Bind();
    glUniform4iv(CachedUniformLocation(sUni), 1, data);
}

void Mesh::InitUniform3i(const char* uniform, const GLint* data) {
    std::string sUni(uniform);
    if (!CacheUniform(sUni, (void*)data, 3 * sizeof(GLint))) return;
    this->shader.Bind();
    glUniform3iv(CachedUniformLocation(sUni), 1, data);
}

void Mesh::InitUniform2i(const char* uniform, const GLint* data) {
    std::string sUni(uniform);
    if (!CacheUniform(sUni, (void*)data, 2 * sizeof(GLint))) return;
    this->shader.Bind();
    glUniform2iv(CachedUniformLocation(sUni), 1, data);
}

void Mesh::InitUniform1i(const char* uniform, const GLint* data) {
    std::string sUni(uniform);
    if (!CacheUniform(sUni, (void*)data, sizeof(GLint))) return;
    this->shader.Bind();
    glUniform1iv(CachedUniformLocation(sUni), 1, data);
}

void Mesh::InitUniformMatrix4f(const char* uniform, const GLfloat* data) {
    std::string sUni(uniform);
    if (!CacheUniform(sUni, (void*)data, 4 * 4 * sizeof(GLfloat))) return;
    this->shader.Bind();
    glUniformMatrix4fv(CachedUniformLocation(sUni), 1, GL_FALSE, data);
}

GLint Mesh::CachedUniformLocation(const std::string& uniform) {
    auto& cache = this->uniformCache[uniform];
    GLuint ID = this->shader.GetID();
    if (cache.location != -2 && cache.shaderID == ID) return cache.location;
    cache.shaderID = ID;
    cache.location = glGetUniformLocation(ID, uniform.c_str());
    if (cache.location == -1) LOG_ERROR(1, "Uniform ", uniform, " not found");
    return cache.location;
}

bool Mesh::CacheUniform(const std::string& uniform, void* data, size_t size) {
    auto& cache = this->uniformCache[uniform];
    size = size > 64 ? 64 : size;
    if (cache.location > -1 && cache.shaderID == this->shader.GetID() && cache.size == size && memcmp(cache.data.data(), data, size) == 0) 
        return false;
        
    cache.size = size;
    memcpy(cache.data.data(), data, size);

    return true;
}

void Mesh::FreeCache() {
    this->uniformCache.clear();
}
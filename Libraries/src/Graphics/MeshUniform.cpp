#include "Mesh.h"

void Mesh::SetUniform4f(const char *uniform, const float *data)
{
    std::string sUni(uniform);
    if (!CacheUniform(sUni, (void *)data, 4 * sizeof(float)))
    {
        return;
    }
    this->shaderProgram.Bind();
    this->shaderProgram.SetUniformFloats(CachedUniformLocation(sUni), data, 4);
}

void Mesh::SetUniform3f(const char *uniform, const float *data)
{
    std::string sUni(uniform);
    if (!CacheUniform(sUni, (void *)data, 3 * sizeof(float)))
    {
        return;
    }
    this->shaderProgram.Bind();
    this->shaderProgram.SetUniformFloats(CachedUniformLocation(sUni), data, 3);
}

void Mesh::SetUniform2f(const char *uniform, const float *data)
{
    std::string sUni(uniform);
    if (!CacheUniform(sUni, (void *)data, 2 * sizeof(float)))
    {
        return;
    }
    this->shaderProgram.Bind();
    this->shaderProgram.SetUniformFloats(CachedUniformLocation(sUni), data, 2);
}

void Mesh::SetUniform1f(const char *uniform, const float *data)
{
    std::string sUni(uniform);
    if (!CacheUniform(sUni, (void *)data, sizeof(float)))
    {
        return;
    }
    this->shaderProgram.Bind();
    this->shaderProgram.SetUniformFloats(CachedUniformLocation(sUni), data, 1);
}

void Mesh::SetUniform4i(const char *uniform, const int *data)
{
    std::string sUni(uniform);
    if (!CacheUniform(sUni, (void *)data, 4 * sizeof(int)))
    {
        return;
    }
    this->shaderProgram.Bind();
    this->shaderProgram.SetUniformInts(CachedUniformLocation(sUni), data, 4);
}

void Mesh::SetUniform3i(const char *uniform, const int *data)
{
    std::string sUni(uniform);
    if (!CacheUniform(sUni, (void *)data, 3 * sizeof(int)))
    {
        return;
    }
    this->shaderProgram.Bind();
    this->shaderProgram.SetUniformInts(CachedUniformLocation(sUni), data, 3);
}

void Mesh::SetUniform2i(const char *uniform, const int *data)
{
    std::string sUni(uniform);
    if (!CacheUniform(sUni, (void *)data, 2 * sizeof(int)))
    {
        return;
    }
    this->shaderProgram.Bind();
    this->shaderProgram.SetUniformInts(CachedUniformLocation(sUni), data, 2);
}

void Mesh::SetUniform1i(const char *uniform, const int *data)
{
    std::string sUni(uniform);
    if (!CacheUniform(sUni, (void *)data, sizeof(int)))
    {
        return;
    }
    this->shaderProgram.Bind();
    this->shaderProgram.SetUniformInts(CachedUniformLocation(sUni), data, 1);
}

void Mesh::SetUniformMatrix4f(const char *uniform, const float *data)
{
    std::string sUni(uniform);
    if (!CacheUniform(sUni, (void *)data, 4 * 4 * sizeof(float)))
    {
        return;
    }
    this->shaderProgram.Bind();
    this->shaderProgram.SetUniformMatrix4(CachedUniformLocation(sUni), data);
}

std::unordered_map<std::string, Mesh::UniformCache> &Mesh::GetOrCreateUniformCache()
{
    if (!this->uniformCache)
    {
        this->uniformCache = std::make_unique<std::unordered_map<std::string, UniformCache>>();
    }
    return *this->uniformCache;
}

int Mesh::CachedUniformLocation(const std::string &uniform)
{
    auto &cache = GetOrCreateUniformCache()[uniform];
    std::uint32_t ID = this->shaderProgram.GetID();
    if (cache.location != -2 && cache.shaderProgramID == ID)
    {
        return cache.location;
    }
    cache.shaderProgramID = ID;
    cache.location = this->shaderProgram.GetUniformLocation(uniform);
    return cache.location;
}

bool Mesh::CacheUniform(const std::string &uniform, void *data, size_t size)
{
    auto &cache = GetOrCreateUniformCache()[uniform];
    size = size > 64 ? 64 : size;
    if (cache.location > -1 && cache.shaderProgramID == this->shaderProgram.GetID() && cache.size == size &&
        memcmp(cache.data.data(), data, size) == 0)
    {
        return false;
    }

    cache.size = size;
    memcpy(cache.data.data(), data, size);

    return true;
}

void Mesh::FreeCache()
{
    if (this->uniformCache)
    {
        this->uniformCache->clear();
    }
}

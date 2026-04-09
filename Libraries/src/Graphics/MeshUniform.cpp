#include "Mesh.h"

void Mesh::InitUniform4f(const char *uniform, const float *data)
{
    std::string sUni(uniform);
    if (!CacheUniform(sUni, (void *)data, 4 * sizeof(float)))
    {
        return;
    }
    this->shaderProgram.Bind();
    this->shaderProgram.SetUniformFloats(CachedUniformLocation(sUni), data, 4);
}

void Mesh::InitUniform3f(const char *uniform, const float *data)
{
    std::string sUni(uniform);
    if (!CacheUniform(sUni, (void *)data, 3 * sizeof(float)))
    {
        return;
    }
    this->shaderProgram.Bind();
    this->shaderProgram.SetUniformFloats(CachedUniformLocation(sUni), data, 3);
}

void Mesh::InitUniform2f(const char *uniform, const float *data)
{
    std::string sUni(uniform);
    if (!CacheUniform(sUni, (void *)data, 2 * sizeof(float)))
    {
        return;
    }
    this->shaderProgram.Bind();
    this->shaderProgram.SetUniformFloats(CachedUniformLocation(sUni), data, 2);
}

void Mesh::InitUniform1f(const char *uniform, const float *data)
{
    std::string sUni(uniform);
    if (!CacheUniform(sUni, (void *)data, sizeof(float)))
    {
        return;
    }
    this->shaderProgram.Bind();
    this->shaderProgram.SetUniformFloats(CachedUniformLocation(sUni), data, 1);
}

void Mesh::InitUniform4i(const char *uniform, const int *data)
{
    std::string sUni(uniform);
    if (!CacheUniform(sUni, (void *)data, 4 * sizeof(int)))
    {
        return;
    }
    this->shaderProgram.Bind();
    this->shaderProgram.SetUniformInts(CachedUniformLocation(sUni), data, 4);
}

void Mesh::InitUniform3i(const char *uniform, const int *data)
{
    std::string sUni(uniform);
    if (!CacheUniform(sUni, (void *)data, 3 * sizeof(int)))
    {
        return;
    }
    this->shaderProgram.Bind();
    this->shaderProgram.SetUniformInts(CachedUniformLocation(sUni), data, 3);
}

void Mesh::InitUniform2i(const char *uniform, const int *data)
{
    std::string sUni(uniform);
    if (!CacheUniform(sUni, (void *)data, 2 * sizeof(int)))
    {
        return;
    }
    this->shaderProgram.Bind();
    this->shaderProgram.SetUniformInts(CachedUniformLocation(sUni), data, 2);
}

void Mesh::InitUniform1i(const char *uniform, const int *data)
{
    std::string sUni(uniform);
    if (!CacheUniform(sUni, (void *)data, sizeof(int)))
    {
        return;
    }
    this->shaderProgram.Bind();
    this->shaderProgram.SetUniformInts(CachedUniformLocation(sUni), data, 1);
}

void Mesh::InitUniformMatrix4f(const char *uniform, const float *data)
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
    if (cache.location == -1)
    {
        LOG_ERROR(1, "Uniform ", uniform, " not found");
    }
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

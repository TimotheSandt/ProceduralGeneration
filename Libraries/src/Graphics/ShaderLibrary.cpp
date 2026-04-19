#include "ShaderLibrary.h"

#include "Logger.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// Disk cache format
//   [4]  magic        "SHBC"
//   [4]  version      uint32_t  (current: 1)
//   [8]  sourceHash   uint64_t  FNV-1a of vertex+fragment source
//   [4]  binaryFormat uint32_t  GL_PROGRAM_BINARY_FORMAT
//   [4]  binaryLen    uint32_t
//   [N]  binary       raw bytes
// ---------------------------------------------------------------------------
static constexpr std::uint32_t CACHE_MAGIC   = 0x43424853u;  // "SHBC"
static constexpr std::uint32_t CACHE_VERSION = 1u;

// ---------------------------------------------------------------------------
// ShaderLibrary
// ---------------------------------------------------------------------------
ShaderLibrary &ShaderLibrary::Instance()
{
    static ShaderLibrary instance;
    return instance;
}

std::string ShaderLibrary::MakeCacheKey(const char *vertexPath, const char *fragmentPath) const
{
    std::string key;
    key.reserve(256);
    if (vertexPath != nullptr)
        key += vertexPath;
    key += '\0';
    if (fragmentPath != nullptr)
        key += fragmentPath;
    return key;
}

const ShaderProgram &ShaderLibrary::Get(const char *vertexPath, const char *fragmentPath)
{
    const std::string key = MakeCacheKey(vertexPath, fragmentPath);

    auto it = registry.find(key);
    if (it != registry.end())
    {
        return it->second;
    }

    ShaderProgram shader;

    if (binaryCacheEnabled)
    {
        // Read sources to compute hash (needed even when loading from cache to validate).
        std::string vertexSource;
        std::string fragmentSource;

        try { vertexSource   = get_file_contents(vertexPath);   } catch (...) {}
        try { fragmentSource = get_file_contents(fragmentPath); } catch (...) {}

        if (!TryLoadFromDiskCache(shader, vertexSource, fragmentSource))
        {
            shader.SetShaderCode(vertexSource, fragmentSource);
            if (shader.IsCompiled())
            {
                SaveToDiskCache(shader, vertexSource, fragmentSource);
            }
        }
    }
    else
    {
        shader.SetShader(vertexPath, fragmentPath);
    }

    auto result = registry.emplace(key, std::move(shader));
    return result.first->second;
}

void ShaderLibrary::PreWarm(std::initializer_list<std::pair<const char *, const char *>> shaders)
{
    for (auto [vert, frag] : shaders)
    {
        Get(vert, frag);
    }
}

void ShaderLibrary::SetBinaryCacheEnabled(bool enabled)
{
    binaryCacheEnabled = enabled;
}

void ShaderLibrary::SetBinaryCacheDirectory(std::string directory)
{
    cacheDirectory = directory;
}

void ShaderLibrary::Clear()
{
    registry.clear();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------
std::uint64_t ShaderLibrary::HashSources(const std::string &vertexSource, const std::string &fragmentSource)
{
    // Hash vertex then fragment consecutively.
    std::uint64_t hash = 14695981039346656037ull;
    auto mix = [&](const std::string &s) {
        for (unsigned char c : s)
        {
            hash ^= c;
            hash *= 1099511628211ull;
        }
        // Separator so "AB"+"C" != "A"+"BC"
        hash ^= 0xFFu;
        hash *= 1099511628211ull;
    };
    mix(vertexSource);
    mix(fragmentSource);
    return hash;
}

std::string ShaderLibrary::MakeCacheFilePath(std::uint64_t hash) const
{
    // e.g.  res/cache/shaders/a3f0c2e1b4d56789.shbc
    std::ostringstream oss;
    oss << cacheDirectory << '/';
    oss << std::hex;
    oss.width(16);
    oss.fill('0');
    oss << hash << ".shbc";
    return oss.str();
}

bool ShaderLibrary::TryLoadFromDiskCache(ShaderProgram &out, const std::string &vertexSource, const std::string &fragmentSource)
{
    const std::uint64_t hash     = HashSources(vertexSource, fragmentSource);
    const std::string   filePath = MakeCacheFilePath(hash);

    std::ifstream file(filePath, std::ios::binary);
    if (!file)
    {
        return false;
    }

    std::uint32_t magic   = 0;
    std::uint32_t version = 0;
    std::uint64_t stored  = 0;

    file.read(reinterpret_cast<char *>(&magic),   sizeof(magic));
    file.read(reinterpret_cast<char *>(&version), sizeof(version));
    file.read(reinterpret_cast<char *>(&stored),  sizeof(stored));

    if (!file || magic != CACHE_MAGIC || version != CACHE_VERSION || stored != hash)
    {
        return false;
    }

    if (!out.LoadBinary(file))
    {
        LOG_ERROR(1, "ShaderLibrary: binary cache hit but LoadBinary failed for hash ", hash, " — recompiling");
        return false;
    }

    return true;
}

void ShaderLibrary::SaveToDiskCache(const ShaderProgram &shader, const std::string &vertexSource, const std::string &fragmentSource)
{
    const std::uint64_t hash     = HashSources(vertexSource, fragmentSource);
    const std::string   filePath = MakeCacheFilePath(hash);

    std::error_code ec;
    std::filesystem::create_directories(cacheDirectory, ec);
    if (ec)
    {
        LOG_ERROR(1, "ShaderLibrary: could not create cache directory '", cacheDirectory, "': ", ec.message());
        return;
    }

    std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        LOG_ERROR(1, "ShaderLibrary: could not open cache file for writing: ", filePath);
        return;
    }

    file.write(reinterpret_cast<const char *>(&CACHE_MAGIC),   sizeof(CACHE_MAGIC));
    file.write(reinterpret_cast<const char *>(&CACHE_VERSION), sizeof(CACHE_VERSION));
    file.write(reinterpret_cast<const char *>(&hash),          sizeof(hash));

    if (!shader.SaveBinary(file))
    {
        LOG_ERROR(1, "ShaderLibrary: SaveBinary failed for hash ", hash, " — cache not written");
        // Remove the partial file so it is not picked up as valid next time.
        file.close();
        std::filesystem::remove(filePath, ec);
    }
}

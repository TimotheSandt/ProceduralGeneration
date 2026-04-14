#pragma once

#include "Graphics/ShaderProgram.h"

#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

// Singleton shader registry with optional disk binary cache.
//
// Usage:
//   ShaderLibrary::Instance().PreWarm({{"res/vert.glsl", "res/frag.glsl"}});
//   const ShaderProgram &shader = ShaderLibrary::Instance().Get("res/vert.glsl", "res/frag.glsl");
class ShaderLibrary
{
  public:
    static ShaderLibrary &Instance();

    // Returns a compiled ShaderProgram. Compiles on first call; subsequent calls
    // return the cached program (shared_ptr — no extra GPU allocation).
    const ShaderProgram &Get(const char *vertexPath, const char *fragmentPath);

    // Compile several shaders up front (e.g. at startup). Silently skips already-cached entries.
    void PreWarm(std::initializer_list<std::pair<const char *, const char *>> shaders);

    // Binary cache control. Disabled by default.
    void SetBinaryCacheEnabled(bool enabled);
    void SetBinaryCacheDirectory(std::string_view directory);

    // Remove all in-memory entries (GPU programs are freed when no ShaderProgram holds a reference).
    void Clear();

  private:
    ShaderLibrary() = default;
    ShaderLibrary(const ShaderLibrary &) = delete;
    ShaderLibrary &operator=(const ShaderLibrary &) = delete;

    std::string MakeCacheKey(const char *vertexPath, const char *fragmentPath) const;
    std::string MakeCacheFilePath(std::uint64_t hash) const;

    static std::uint64_t HashSources(const std::string &vertexSource, const std::string &fragmentSource);

    bool TryLoadFromDiskCache(ShaderProgram &out, const std::string &vertexSource, const std::string &fragmentSource);
    void SaveToDiskCache(const ShaderProgram &shader, const std::string &vertexSource, const std::string &fragmentSource);

    std::unordered_map<std::string, ShaderProgram> registry;

    bool binaryCacheEnabled = false;
    std::string cacheDirectory = "res/cache/shaders";
};

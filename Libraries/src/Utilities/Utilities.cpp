#include "utilities.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Logger.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace
{
fs::path EnsureAppDirectory(const fs::path &basePath)
{
    std::error_code ec;
    fs::path appPath = basePath / "ProceduralGeneration";
    fs::create_directories(appPath, ec);
    return appPath;
}
} // namespace

float lerp(float a, float b, float t) { return a + t * (b - a); }

void rotate(float &x, float &y, float angle)
{
    glm::vec2 v = glm::rotate(glm::vec2(x, y), angle);
    x = v.x;
    y = v.y;
}

void rotate(float &x, float &y, float &z, float angle)
{
    glm::vec3 axis = glm::normalize(glm::vec3(1, 1, 1));
    glm::quat q = glm::angleAxis(angle, axis);
    glm::vec3 rotatedQuat = q * glm::vec3(x, y, z);
    x = rotatedQuat.x;
    y = rotatedQuat.y;
    z = rotatedQuat.z;
}

void rotate(float &x, float &y, float &z, float &w, float angle)
{
    glm::mat4 rotXY(1.0f);
    rotXY[0][0] = std::cos(angle);
    rotXY[0][1] = std::sin(angle);
    rotXY[1][0] = -std::sin(angle);
    rotXY[1][1] = std::cos(angle);

    glm::mat4 rotZW(1.0f);
    rotZW[2][2] = std::cos(angle);
    rotZW[2][3] = std::sin(angle);
    rotZW[3][2] = -std::sin(angle);
    rotZW[3][3] = std::cos(angle);

    glm::mat4 combinedRot = rotZW * rotXY;
    glm::vec4 rotatedCombined = combinedRot * glm::vec4(x, y, z, w);
    x = rotatedCombined.x;
    y = rotatedCombined.y;
    z = rotatedCombined.z;
    w = rotatedCombined.w;
}

std::string GetExecutablePath()
{
#ifdef _WIN32
    std::vector<char> buffer(MAX_PATH, '\0');

    while (true)
    {
        DWORD length = GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
        {
            return {};
        }
        if (length < buffer.size() - 1)
        {
            return std::string(buffer.data(), length);
        }
        buffer.resize(buffer.size() * 2, '\0');
    }
#elif defined(__APPLE__)
    uint32_t bufferSize = 0;
    char probeBuffer[1];
    _NSGetExecutablePath(probeBuffer, &bufferSize);
    std::vector<char> buffer(bufferSize, '\0');
    if (_NSGetExecutablePath(buffer.data(), &bufferSize) != 0)
    {
        return {};
    }

    std::error_code ec;
    fs::path canonicalPath = fs::weakly_canonical(fs::path(buffer.data()), ec);
    return ec ? std::string(buffer.data()) : canonicalPath.string();
#else
    std::vector<char> buffer(1024, '\0');

    while (true)
    {
        const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size());
        if (length < 0)
        {
            return {};
        }
        if (static_cast<size_t>(length) < buffer.size())
        {
            return std::string(buffer.data(), static_cast<size_t>(length));
        }
        buffer.resize(buffer.size() * 2, '\0');
    }
#endif
}

std::string GetExecutableDirectory()
{
    const std::string executablePath = GetExecutablePath();
    if (executablePath.empty())
    {
        return {};
    }
    return fs::path(executablePath).parent_path().string();
}

void SetWorkingDirectoryToExe()
{
    const std::string executableDirectory = GetExecutableDirectory();
    if (executableDirectory.empty())
    {
        return;
    }

    std::error_code ec;
    fs::current_path(fs::path(executableDirectory), ec);
    if (ec)
    {
        LOG_ERROR(1, "Failed to set working directory to executable directory: ", ec.message());
    }
}

std::string GetUserDataPath()
{
    fs::path dataDirectory;

#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path)))
    {
        dataDirectory = EnsureAppDirectory(fs::path(path));
    }
#elif defined(__APPLE__)
    if (const char *home = std::getenv("HOME"))
    {
        dataDirectory = EnsureAppDirectory(fs::path(home) / "Library" / "Application Support");
    }
#else
    if (const char *xdgDataHome = std::getenv("XDG_DATA_HOME"))
    {
        dataDirectory = EnsureAppDirectory(fs::path(xdgDataHome));
    }
    else if (const char *home = std::getenv("HOME"))
    {
        dataDirectory = EnsureAppDirectory(fs::path(home) / ".local" / "share");
    }
#endif

    if (dataDirectory.empty())
    {
        return {};
    }

    std::string pathString = dataDirectory.string();
    if (!pathString.empty() && pathString.back() != fs::path::preferred_separator)
    {
        pathString.push_back(fs::path::preferred_separator);
    }
    return pathString;
}

bool ValidateAssets(const std::vector<std::string> &assetPaths, std::vector<std::string> *missingAssets)
{
    bool allAssetsPresent = true;

    if (missingAssets != nullptr)
    {
        missingAssets->clear();
    }

    for (const std::string &assetPathString : assetPaths)
    {
        const fs::path assetPath(assetPathString);
        std::error_code ec;
        const bool exists = fs::exists(assetPath, ec);

        if (!exists || ec)
        {
            allAssetsPresent = false;
            if (missingAssets != nullptr)
            {
                missingAssets->push_back(assetPath.generic_string());
            }
            LOG_ERROR(1, "Missing asset: ", assetPath.generic_string());
        }
    }

    return allAssetsPresent;
}

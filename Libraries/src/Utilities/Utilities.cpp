#include "utilities.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>


#ifdef _WIN32
#include <windows.h>
#include <shlobj.h> // for SHGetFolderPath
#include <direct.h> // for _chdir
std::string dirname(const std::string& path) {
    size_t pos = path.find_last_of("\\/");
    if (pos == std::string::npos) return "";
    return path.substr(0, pos);
}
#else
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h> // for strdup
#include <libgen.h> // for dirname
#endif

float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

void rotate(float& x, float& y, float angle) {
    glm::vec2 v = glm::rotate(glm::vec2(x, y), angle);
    x = v.x;
    y = v.y;
}

void rotate(float& x, float& y, float& z, float angle) {
    glm::vec3 axis = glm::normalize(glm::vec3(1, 1, 1));
    glm::quat q = glm::angleAxis(angle, axis);
    glm::vec3 rotatedQuat = q * glm::vec3(x, y, z);
    x = rotatedQuat.x;
    y = rotatedQuat.y;
    z = rotatedQuat.z;
}

void rotate(float& x, float& y, float& z, float& w, float angle) {
    glm::mat4 rotXY(1.0f);
    rotXY[0][0] = cos(angle);  rotXY[0][1] = sin(angle);
    rotXY[1][0] = -sin(angle); rotXY[1][1] = cos(angle);
    
    // Rotation in ZW plane
    glm::mat4 rotZW(1.0f);
    rotZW[2][2] = cos(angle);  rotZW[2][3] = sin(angle);
    rotZW[3][2] = -sin(angle); rotZW[3][3] = cos(angle);
    
    // Combined rotation
    glm::mat4 combinedRot = rotZW * rotXY;
    glm::vec4 rotatedCombined = combinedRot * glm::vec4(x, y, z, w);
    x = rotatedCombined.x;
    y = rotatedCombined.y;
    z = rotatedCombined.z;
    w = rotatedCombined.w;
}

std::string GetExecutablePath() {
#ifdef _WIN32
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string exePath(path);
    return exePath;
#else
    char path[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
    if (count != -1) {
        path[count] = '\0';
    }
    return std::string(path);
#endif
}

void SetWorkingDirectoryToExe() {
    
#ifdef _WIN32
    std::string exePath = GetExecutablePath();
    std::string dir = dirname(exePath);
    _chdir(dir.c_str());
#else
    const char* exePathStr = GetExecutablePath().c_str();
    char* dirStr = strdup(exePathStr);
    char* dir = dirname(dirStr);
    if (chdir(dir) != 0) {
        perror("chdir");
    }
    free(dirStr);
#endif
}

std::string GetUserDataPath() {
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        std::string appDataPath(path);
        std::string logDir = appDataPath + "\\ProceduralGeneration\\";
        _mkdir(logDir.c_str()); // Create if not exists
        return logDir;
    }
    return "";
#else
    const char* home = getenv("HOME");
    if (home) {
        std::string logDir = std::string(home) + "/.local/share/ProceduralGeneration/";
        mkdir(logDir.c_str(), 0755); // Create if not exists
        return logDir;
    }
    return "";
#endif
}
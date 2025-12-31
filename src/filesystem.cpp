#include "filesystem.h"
#include <cstdio>
#include <filesystem>

// Emscripten Specific Includes
#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
#endif

// Platform-specific includes for executable path
#ifdef _WIN32
    #include <windows.h>
#elif __linux__
    #include <unistd.h>
    #include <linux/limits.h>
#elif __APPLE__
    #include <mach-o/dyld.h>
    #include <limits.h>
#endif

std::filesystem::path executable_path;

std::filesystem::path getExecutableDirectory() {
    // Web check: If we are on the web, we don't need to search paths
#ifdef __EMSCRIPTEN__
    executable_path = std::filesystem::path("/"); 
    return executable_path;
#endif

    // --- YOUR EXISTING NATIVE LOGIC ---
    std::filesystem::path execPath;
#ifdef _WIN32
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    execPath = std::filesystem::path(buffer).parent_path();
#elif __APPLE__
    char buffer[PATH_MAX];
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) == 0) {
        execPath = std::filesystem::path(buffer).parent_path();
    }
#elif __linux__
    char buffer[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", buffer, PATH_MAX);
    execPath = std::filesystem::path(std::string(buffer, (count > 0) ? count : 0)).parent_path();
#endif

    executable_path = execPath;

    // Search for CMakeLists.txt to identify dev environment
    std::filesystem::path searchPath = executable_path;
    for (int i = 0; i < 5; i++) {
        if (std::filesystem::exists(searchPath / "CMakeLists.txt")) {
            executable_path = searchPath;
            break;
        }
        if (searchPath.has_parent_path()) searchPath = searchPath.parent_path();
        else break;
    }
    
    return executable_path;
}

std::filesystem::path getExecutablePath() {
    return getExecutableDirectory();
}

std::string buildAssetPath(const std::string& relative_path) {
    // Ensure executable_path is initialized
    if (executable_path.empty()) {
        getExecutableDirectory();
    }

#ifdef __EMSCRIPTEN__
    // On web, we just return the relative path because the root is "/"
    return relative_path; 
#else
    std::filesystem::path fullPath = executable_path / relative_path;
    return fullPath.string();
#endif
}

std::string resolveTexturePath(const std::string& modelPath, const std::string& textureName) {
    std::filesystem::path mPath(modelPath);
    std::filesystem::path tPath(textureName);
    
    // If the texture name is already an absolute path or has its own dir
    if (tPath.is_absolute()) return textureName;

    // Return texture relative to the model's directory
    return (mPath.parent_path() / tPath).string();
}
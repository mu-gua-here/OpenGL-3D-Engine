#pragma once

#include <string>
#include <filesystem>

// Filesystem operations
std::filesystem::path getExecutablePath();
std::string buildAssetPath(const std::string& relative_path);
std::string resolveTexturePath(const std::string& modelPath, const std::string& textureName);

extern std::filesystem::path executable_path;
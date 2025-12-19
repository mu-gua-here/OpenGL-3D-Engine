#include <string>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <filesystem>
#include "filesystem.h"

// ============================================================================
// SHADER LOADING FUNCTIONS
// ============================================================================

std::string loadShaderFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        printf("Error: Could not open shader file: %s\n", path.c_str());
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string shader_content = buffer.str();
    
    // Remove existing #version directive if present
    size_t version_pos = shader_content.find("#version");
    if (version_pos != std::string::npos) {
        size_t newline_pos = shader_content.find('\n', version_pos);
        if (newline_pos != std::string::npos) {
            shader_content.erase(version_pos, newline_pos - version_pos + 1);
        }
    }
    
    // Prepend correct version for platform
    std::string version_string;
    #ifdef __EMSCRIPTEN__
        version_string = "#version 300 es\n"
                        "precision mediump float;\n"
                        "precision lowp sampler2D;\n"
                        "precision lowp sampler2DShadow;\n"
                        "precision lowp samplerCube;\n";
    #else
        version_string = "#version 330 core\n";
    #endif
    
    return version_string + shader_content;
}

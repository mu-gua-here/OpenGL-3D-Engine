#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <cstdio>

// Forward declaration
extern GLuint default_texture_id;

enum AlphaMode {
    OPAQUE,   // No transparency - fastest
    MASKED,   // Alpha cutout - No sorting needed
    BLEND     // True transparency - Needs sorting
};

class Material {
public:
    std::string name;
    AlphaMode alphaMode = OPAQUE;
    
    // PBR properties with proper defaults
    glm::vec3 base_color{1.0f, 1.0f, 1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float ao = 1.0f;
    glm::vec3 emissive{0.0f, 0.0f, 0.0f};
    
    GLuint albedo_map = 0;
    GLuint normal_map = 0;
    GLuint orm_map = 0; // AO, roughness, metallic and height packed
    GLuint height_map = 0;
    GLuint emissive_map = 0;
    GLuint specular_map = 0;
    
    float height_scale = 0.01f;
    bool invert_height = false;
    
    Material() = default;
    Material(const std::string& name) : name(name) {}
    
    bool hasAlbedoMap() const { return albedo_map != 0; }
    bool hasNormalMap() const { return normal_map != 0; }
    bool hasORMMap() const { return orm_map != 0; }
    bool hasEmissiveMap() const { return emissive_map != 0; }
    bool hasHeightMap() const { return height_map != 0; }
    bool hasSpecularMap() const { return specular_map != 0; }
};

Material createDefaultMaterial();

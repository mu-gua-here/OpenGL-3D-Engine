#pragma once

#include <string>
#include <vector>
#include <memory>
#include <assimp/scene.h>
#include "mesh.h"
#include "material.h"

// Texture loading functions
GLuint loadTexture(const std::string& path);
GLuint loadTextureFromMemory(unsigned char* data, unsigned int size);
GLuint loadTextureFromARGB(aiTexel* data, unsigned int width, unsigned int height);
GLuint loadCubemap(const char* faces[6]);

// Asset loading functions
std::vector<std::unique_ptr<Mesh>> loadMesh(const std::string& filepath);
Material createMaterialFromAssimp(std::string modelPath, aiMaterial* material, const aiScene* scene);

// Path utilities
std::string buildAssetPath(const std::string& relative_path);
std::string resolveTexturePath(const std::string& modelPath, const std::string& textureName);

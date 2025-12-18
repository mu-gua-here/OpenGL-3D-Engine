#pragma once

#include <assimp/scene.h>
#include "material.h"
#include <string>
#include <vector>
#include <memory>

class Mesh;

struct ORMResult {
    GLuint textureID;
    bool hasHeightData;
};

unsigned char* load_greyscale_data(const std::string& path, const aiScene* scene, int& width, int& height);

ORMResult packORM(Material& mat, const std::string& current_material_name, const std::string& ao_path,
                  const std::string& roughness_path, const std::string& metallic_path, const std::string& height_path,
                  const std::string& specular_path, const aiScene* scene);

Material createMaterialFromAssimp(std::string modelPath, aiMaterial* material, const aiScene* scene);

std::vector<std::unique_ptr<Mesh>> loadMesh(const std::string& filepath);

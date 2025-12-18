#include "mesh_loader.h"
#include "filesystem.h"
#include "asset_loader.h"
#include "material.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <algorithm>
#include <functional>
#include <cstdio>

#include "stb_image.h"

unsigned char* load_greyscale_data(const std::string& path, const aiScene* scene, int& width, int& height) {
    if (!path.empty() && path[0] == '*') {
        int texIndex = std::atoi(path.c_str() + 1);
        if (texIndex >= 0 && texIndex < (int)scene->mNumTextures) {
            aiTexture* embeddedTex = scene->mTextures[texIndex];
            if (embeddedTex->mHeight == 0) {
                int channels;
                unsigned char* data = stbi_load_from_memory((unsigned char*)embeddedTex->pcData, embeddedTex->mWidth, &width, &height, &channels, 1);
                return data;
            } else {
                width = embeddedTex->mWidth;
                height = embeddedTex->mHeight;
                unsigned char* data = new unsigned char[width * height];
                for (int i = 0; i < width * height; ++i) {
                    float r = embeddedTex->pcData[i].r / 255.0f;
                    float g = embeddedTex->pcData[i].g / 255.0f;
                    float b = embeddedTex->pcData[i].b / 255.0f;
                    float gray = 0.299f * r + 0.587f * g + 0.114f * b;
                    data[i] = (unsigned char)(gray * 255.0f);
                }
                return data;
            }
        }
    }
    if (!path.empty()) {
        int channels;
        return stbi_load(path.c_str(), &width, &height, &channels, 1);
    }
    return nullptr;
}

ORMResult packORM(Material& mat, const std::string& current_material_name, const std::string& ao_path,
                  const std::string& roughness_path, const std::string& metallic_path, const std::string& height_path,
                  const std::string& specular_path, const aiScene* scene) {
    int w_ao, h_ao, w_r, h_r, w_m, h_m, w_h, h_h, w_s, h_s;
    unsigned char* ao_data = load_greyscale_data(ao_path, scene, w_ao, h_ao);
    unsigned char* roughness_data = load_greyscale_data(roughness_path, scene, w_r, h_r);
    unsigned char* metallic_data = load_greyscale_data(metallic_path, scene, w_m, h_m);
    unsigned char* height_data = load_greyscale_data(height_path, scene, w_h, h_h);
    unsigned char* specular_data = load_greyscale_data(specular_path, scene, w_s, h_s);
    
    if (!ao_data && !roughness_data && !metallic_data && !height_data && !specular_data) return { 0, false };

    int width = 0, height = 0;
    if (ao_data) { width = w_ao; height = h_ao; }
    else if (roughness_data) { width = w_r; height = h_r; }
    else if (metallic_data) { width = w_m; height = h_m; }
    else if (height_data) { width = w_h; height = h_h; }
    else if (specular_data) { width = w_s; height = h_s; }
    
    if (width == 0 || height == 0) return { 0, false };

    size_t data_size = (size_t)width * height * 4;
    unsigned char* packed_data = new unsigned char[data_size];
    unsigned char* default_channel_data = new unsigned char[width * height];
    
    unsigned char* final_ao = ao_data;
    if (!ao_data) { memset(default_channel_data, 255, width * height); final_ao = default_channel_data; }
    
    unsigned char* final_roughness = roughness_data;
    if (!roughness_data && specular_data) {
        final_roughness = new unsigned char[width * height];
        for (int i = 0; i < width * height; ++i) final_roughness[i] = 255 - specular_data[i];
    } else if (!roughness_data) {
        memset(default_channel_data, 255, width * height);
        final_roughness = default_channel_data;
    }
    
    unsigned char* final_metallic = metallic_data;
    if (!metallic_data) { memset(default_channel_data, 0, width * height); final_metallic = default_channel_data; }
    
    bool hasHeightData = (height_data != nullptr);
    unsigned char* final_height = height_data;
    if (height_data && mat.invert_height) {
        final_height = new unsigned char[width * height];
        for (int i = 0; i < width * height; ++i) final_height[i] = 255 - height_data[i];
        printf("Height map for '%s': Inverted during packing\n", current_material_name.c_str());
    } else if (!height_data) {
        memset(default_channel_data, 128, width * height);
        final_height = default_channel_data;
    }

    for (int i = 0; i < width * height; ++i) {
        packed_data[i * 4] = final_ao[i];
        packed_data[i * 4 + 1] = final_roughness[i];
        packed_data[i * 4 + 2] = final_metallic[i];
        packed_data[i * 4 + 3] = final_height[i];
    }
    
    GLuint orm_texture_id;
    glGenTextures(1, &orm_texture_id);
    glBindTexture(GL_TEXTURE_2D, orm_texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, packed_data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (ao_data) stbi_image_free(ao_data);
    if (roughness_data && roughness_data != final_roughness) stbi_image_free(roughness_data);
    if (metallic_data) stbi_image_free(metallic_data);
    if (height_data) {
        stbi_image_free(height_data);
        if (mat.invert_height && final_height != height_data) delete[] final_height;
    }
    if (specular_data) stbi_image_free(specular_data);
    if (!roughness_data && specular_data) delete[] final_roughness;
    delete[] packed_data;
    delete[] default_channel_data;
    
    printf("Successfully created packed ORM map for '%s'%s\n", current_material_name.c_str(), mat.invert_height ? " (height inverted)" : "");
    return { orm_texture_id, hasHeightData };
}

Material createMaterialFromAssimp(std::string modelPath, aiMaterial* material, const aiScene* scene) {
    Material mat = createDefaultMaterial();
    aiString path, matName;
    std::string aoPath, roughPath, metalPath, heightPath, specularPath;
    std::string name = "unnamed";

    if (material->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS) name = matName.C_Str();
        
    float heightScale = 0.1f;
    bool foundHeightScale = false;
    if (material->Get("$tex.bumpmult", aiTextureType_HEIGHT, 0, heightScale) == AI_SUCCESS) foundHeightScale = true;
    if (!foundHeightScale && material->Get("$tex.bumpmult", aiTextureType_NORMALS, 0, heightScale) == AI_SUCCESS) foundHeightScale = true;
    if (!foundHeightScale && material->Get("$tex.bumpmult", aiTextureType_DISPLACEMENT, 0, heightScale) == AI_SUCCESS) foundHeightScale = true;
    if (!foundHeightScale && material->Get(AI_MATKEY_BUMPSCALING, heightScale) == AI_SUCCESS) foundHeightScale = true;
    
    if (foundHeightScale) {
        if (heightScale < 0.0f) { mat.invert_height = true; mat.height_scale = -heightScale; }
        else { mat.invert_height = false; mat.height_scale = heightScale; }
        mat.height_scale = glm::clamp(mat.height_scale, -0.1f, 0.1f);
    }
    
    std::string nameLower = name;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
    if (nameLower.find("_inverted") != std::string::npos || nameLower.find("_inv") != std::string::npos ||
        nameLower.find("_flipheight") != std::string::npos || nameLower.find("invert") != std::string::npos)
        mat.invert_height = true;

    auto resolveTexPath = [&](const aiString& aiPath) -> std::string {
        std::string texPath = aiPath.C_Str();
        if (texPath.empty()) return "";
        if (texPath[0] == '*') return texPath;
        std::filesystem::path modelDir = std::filesystem::path(modelPath).parent_path();
        std::filesystem::path fullTexPath = modelDir / texPath;
        return buildAssetPath("res/scene_models/" + fullTexPath.string());
    };
    
    if (material->GetTexture(aiTextureType_BASE_COLOR, 0, &path) == AI_SUCCESS ||
        material->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS) {
        std::string texPath = resolveTexPath(path);
        if (!texPath.empty() && texPath[0] == '*') {
            int texIndex = std::atoi(texPath.c_str() + 1);
            if (texIndex >= 0 && texIndex < (int)scene->mNumTextures) {
                aiTexture* embeddedTex = scene->mTextures[texIndex];
                if (embeddedTex->mHeight == 0) {
                    mat.albedo_map = loadTextureFromMemory((unsigned char*)embeddedTex->pcData, embeddedTex->mWidth);
                } else {
                    mat.albedo_map = loadTextureFromARGB(embeddedTex->pcData, embeddedTex->mWidth, embeddedTex->mHeight);
                }
            }
        } else if (!texPath.empty()) mat.albedo_map = loadTexture(texPath);
    }
    
    if (!mat.hasAlbedoMap()) {
        aiColor3D color;
        if (material->Get(AI_MATKEY_BASE_COLOR, color) == AI_SUCCESS || material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
            mat.base_color = glm::vec3(color.r, color.g, color.b);
    }
    
    if (material->GetTexture(aiTextureType_NORMALS, 0, &path) == AI_SUCCESS || material->GetTexture(aiTextureType_HEIGHT, 0, &path) == AI_SUCCESS) {
        std::string texPath = resolveTexPath(path);
        if (!texPath.empty() && texPath[0] == '*') {
            int texIndex = std::atoi(texPath.c_str() + 1);
            if (texIndex >= 0 && texIndex < (int)scene->mNumTextures) {
                aiTexture* embeddedTex = scene->mTextures[texIndex];
                if (embeddedTex->mHeight == 0) {
                    mat.normal_map = loadTextureFromMemory((unsigned char*)embeddedTex->pcData, embeddedTex->mWidth);
                } else {
                    mat.normal_map = loadTextureFromARGB(embeddedTex->pcData, embeddedTex->mWidth, embeddedTex->mHeight);
                }
            }
        } else if (!texPath.empty()) mat.normal_map = loadTexture(texPath);
    }
    
    if (material->GetTexture(aiTextureType_METALNESS, 0, &path) == AI_SUCCESS) metalPath = resolveTexPath(path);
    if (metalPath.empty()) {
        float metallicValue;
        if (material->Get(AI_MATKEY_METALLIC_FACTOR, metallicValue) == AI_SUCCESS) mat.metallic = metallicValue;
    }
    
    if (material->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &path) == AI_SUCCESS || material->GetTexture(aiTextureType_SHININESS, 0, &path) == AI_SUCCESS)
        roughPath = resolveTexPath(path);
    if (roughPath.empty() && material->GetTexture(aiTextureType_SPECULAR, 0, &path) == AI_SUCCESS) specularPath = resolveTexPath(path);
    if (roughPath.empty() && specularPath.empty()) {
        float roughnessValue, shininessValue;
        if (material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughnessValue) == AI_SUCCESS) mat.roughness = roughnessValue;
        else if (material->Get(AI_MATKEY_SHININESS, shininessValue) == AI_SUCCESS) mat.roughness = 1.0f - glm::clamp(shininessValue / 1000.0f, 0.0f, 1.0f);
    }
    
    if (material->GetTexture(aiTextureType_AMBIENT_OCCLUSION, 0, &path) == AI_SUCCESS || material->GetTexture(aiTextureType_LIGHTMAP, 0, &path) == AI_SUCCESS ||
        material->GetTexture(aiTextureType_AMBIENT, 0, &path) == AI_SUCCESS) aoPath = resolveTexPath(path);
    
    if (material->GetTexture(aiTextureType_DISPLACEMENT, 0, &path) == AI_SUCCESS || material->GetTexture(aiTextureType_HEIGHT, 0, &path) == AI_SUCCESS) {
        heightPath = resolveTexPath(path);
        std::string heightFileName = std::filesystem::path(heightPath).filename().string();
        std::transform(heightFileName.begin(), heightFileName.end(), heightFileName.begin(), ::tolower);
        if (heightFileName.find("_inv") != std::string::npos || heightFileName.find("inverted") != std::string::npos) {
            mat.invert_height = true;
            printf("Height map '%s': Inversion enabled by filename\n", heightFileName.c_str());
        }
    }
    
    if (!aoPath.empty() || !roughPath.empty() || !metalPath.empty() || !heightPath.empty() || !specularPath.empty()) {
        ORMResult packed = packORM(mat, name, aoPath, roughPath, metalPath, heightPath, specularPath, scene);
        mat.orm_map = packed.textureID;
        if (packed.hasHeightData) mat.height_map = packed.textureID;
    }
    
    if (material->GetTexture(aiTextureType_EMISSIVE, 0, &path) == AI_SUCCESS || material->GetTexture(aiTextureType_EMISSION_COLOR, 0, &path) == AI_SUCCESS) {
        std::string texPath = resolveTexPath(path);
        if (!texPath.empty() && texPath[0] == '*') {
            int texIndex = std::atoi(texPath.c_str() + 1);
            if (texIndex >= 0 && texIndex < (int)scene->mNumTextures) {
                aiTexture* embeddedTex = scene->mTextures[texIndex];
                if (embeddedTex->mHeight == 0) {
                    mat.emissive_map = loadTextureFromMemory((unsigned char*)embeddedTex->pcData, embeddedTex->mWidth);
                } else {
                    mat.emissive_map = loadTextureFromARGB(embeddedTex->pcData, embeddedTex->mWidth, embeddedTex->mHeight);
                }
            }
        } else if (!texPath.empty()) mat.emissive_map = loadTexture(texPath);
    }
    
    if (!mat.hasEmissiveMap()) {
        aiColor3D emissiveColor;
        if (material->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor) == AI_SUCCESS) {
            mat.emissive = glm::vec3(emissiveColor.r, emissiveColor.g, emissiveColor.b);
            float emissiveStrength;
            if (material->Get(AI_MATKEY_EMISSIVE_INTENSITY, emissiveStrength) == AI_SUCCESS) mat.emissive *= emissiveStrength;
        }
    }
    
    mat.name = name;
    return mat;
}

std::vector<std::unique_ptr<Mesh>> loadMesh(const std::string& filepath) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(buildAssetPath("res/scene_models/" + filepath),
        aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality | aiProcess_OptimizeMeshes | aiProcess_CalcTangentSpace | aiProcess_PreTransformVertices);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        printf("Assimp error: %s\n", importer.GetErrorString());
        return {};
    }

    std::vector<std::unique_ptr<Mesh>> meshes;
    std::function<void(aiNode*)> processNode = [&](aiNode* node) {
        for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            auto newMesh = std::make_unique<Mesh>();
            
            for (unsigned int v = 0; v < mesh->mNumVertices; ++v) {
                newMesh->vertices_data.push_back(mesh->mVertices[v].x);
                newMesh->vertices_data.push_back(mesh->mVertices[v].y);
                newMesh->vertices_data.push_back(mesh->mVertices[v].z);
                
                if (mesh->HasVertexColors(0)) {
                    aiColor4D color = mesh->mColors[0][v];
                    newMesh->vertices_data.push_back(color.r);
                    newMesh->vertices_data.push_back(color.g);
                    newMesh->vertices_data.push_back(color.b);
                    newMesh->vertices_data.push_back(color.a);
                } else {
                    newMesh->vertices_data.push_back(1.0f);
                    newMesh->vertices_data.push_back(1.0f);
                    newMesh->vertices_data.push_back(1.0f);
                    newMesh->vertices_data.push_back(1.0f);
                }
                
                if (mesh->HasTextureCoords(0)) {
                    newMesh->vertices_data.push_back(mesh->mTextureCoords[0][v].x);
                    newMesh->vertices_data.push_back(mesh->mTextureCoords[0][v].y);
                } else {
                    newMesh->vertices_data.push_back(0.0f);
                    newMesh->vertices_data.push_back(0.0f);
                }
                
                if (mesh->HasNormals()) {
                    newMesh->vertices_data.push_back(mesh->mNormals[v].x);
                    newMesh->vertices_data.push_back(mesh->mNormals[v].y);
                    newMesh->vertices_data.push_back(mesh->mNormals[v].z);
                } else {
                    newMesh->vertices_data.push_back(0.0f);
                    newMesh->vertices_data.push_back(0.0f);
                    newMesh->vertices_data.push_back(1.0f);
                }
                
                if (mesh->HasTangentsAndBitangents()) {
                    newMesh->vertices_data.push_back(mesh->mTangents[v].x);
                    newMesh->vertices_data.push_back(mesh->mTangents[v].y);
                    newMesh->vertices_data.push_back(mesh->mTangents[v].z);
                } else {
                    newMesh->vertices_data.push_back(1.0f);
                    newMesh->vertices_data.push_back(0.0f);
                    newMesh->vertices_data.push_back(0.0f);
                }
                
                if (mesh->HasTangentsAndBitangents()) {
                    newMesh->vertices_data.push_back(mesh->mBitangents[v].x);
                    newMesh->vertices_data.push_back(mesh->mBitangents[v].y);
                    newMesh->vertices_data.push_back(mesh->mBitangents[v].z);
                } else {
                    newMesh->vertices_data.push_back(0.0f);
                    newMesh->vertices_data.push_back(1.0f);
                    newMesh->vertices_data.push_back(0.0f);
                }
            }
            
            for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
                aiFace face = mesh->mFaces[f];
                for (unsigned int j = 0; j < face.mNumIndices; ++j) newMesh->indices_data.push_back(face.mIndices[j]);
            }
            
            aiMaterial* assimpMat = scene->mMaterials[mesh->mMaterialIndex];
            newMesh->material = createMaterialFromAssimp(filepath, assimpMat, scene);
            newMesh->TRIANGLE_COUNT = mesh->mNumFaces;
            newMesh->INDEX_COUNT = static_cast<unsigned int>(newMesh->indices_data.size());
            
            glGenVertexArrays(1, &newMesh->VAO);
            glGenBuffers(1, &newMesh->VBO);
            glGenBuffers(1, &newMesh->EBO);
            glBindVertexArray(newMesh->VAO);
            glBindBuffer(GL_ARRAY_BUFFER, newMesh->VBO);
            glBufferData(GL_ARRAY_BUFFER, newMesh->vertices_data.size() * sizeof(float), newMesh->vertices_data.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, newMesh->EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, newMesh->indices_data.size() * sizeof(unsigned int), newMesh->indices_data.data(), GL_STATIC_DRAW);

            constexpr GLsizei stride = 18 * sizeof(float);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)(0));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(7 * sizeof(float)));
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(9 * sizeof(float)));
            glEnableVertexAttribArray(4);
            glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride, (void*)(12 * sizeof(float)));
            glEnableVertexAttribArray(5);
            glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, stride, (void*)(15 * sizeof(float)));
            glBindVertexArray(0);

            meshes.push_back(std::move(newMesh));
        }

        for (unsigned int i = 0; i < node->mNumChildren; ++i) processNode(node->mChildren[i]);
    };

    processNode(scene->mRootNode);

    size_t triangles = 0;
    for (const auto& mesh : meshes) triangles += mesh->TRIANGLE_COUNT;
    printf("Loaded mesh from '%s' with %zu sub-meshes and %zu triangles\n", filepath.c_str(), meshes.size(), triangles);

    return meshes;
}

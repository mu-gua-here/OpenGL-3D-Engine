#include "entity_manager.h"
#include "mesh_loader.h"
#include <cstdio>
#include <cmath>

// Global entity manager instance
EntityManager entity_manager;

// Global triangle counter
extern unsigned int total_triangles;

size_t EntityManager::addEntity(Entity&& entity) {
    // Count triangles for this entity
    extern unsigned int total_triangles;
    
    unsigned int entity_triangles = 0;
    for (const auto& mesh : entity.meshes) {
        if (mesh) {
            entity_triangles += mesh->TRIANGLE_COUNT;
        }
    }
    
    total_triangles += entity_triangles;
        
    entities.push_back(std::move(entity));
    return entities.size() - 1;
}

int EntityManager::findEntity(std::string name) {
    for (size_t i = 0; i < entities.size(); i++) {
        if (entities[i].active && entities[i].name == name) {
            return i;
        }
    }
    return -1;
}

bool EntityManager::updateEntity(std::string name, const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scale) {
    int i = findEntity(name);
    if (i == -1) return false;

    const float NO_CHANGE = std::numeric_limits<float>::max();

    // Positions
    if (pos.x != NO_CHANGE) entities[i].position.x = pos.x;
    if (pos.y != NO_CHANGE) entities[i].position.y = pos.y;
    if (pos.z != NO_CHANGE) entities[i].position.z = pos.z;
    
    // Rotations
    if (rot.x != NO_CHANGE) entities[i].rotation.x = rot.x;
    if (rot.y != NO_CHANGE) entities[i].rotation.y = rot.y;
    if (rot.z != NO_CHANGE) entities[i].rotation.z = rot.z;
    
    // Scale
    if (scale.x != NO_CHANGE) entities[i].scale.x = scale.x;
    if (scale.y != NO_CHANGE) entities[i].scale.y = scale.y;
    if (scale.z != NO_CHANGE) entities[i].scale.z = scale.z;

    return true;
}

void EntityManager::updateEntity(size_t index, const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scale) {        

    const float NO_CHANGE = std::numeric_limits<float>::max();

    // Positions
    if (pos.x != NO_CHANGE) entities[index].position.x = pos.x;
    if (pos.y != NO_CHANGE) entities[index].position.y = pos.y;
    if (pos.z != NO_CHANGE) entities[index].position.z = pos.z;
    
    // Rotations
    if (rot.x != NO_CHANGE) entities[index].rotation.x = rot.x;
    if (rot.y != NO_CHANGE) entities[index].rotation.y = rot.y;
    if (rot.z != NO_CHANGE) entities[index].rotation.z = rot.z;
    
    // Scale
    if (scale.x != NO_CHANGE) entities[index].scale.x = scale.x;
    if (scale.y != NO_CHANGE) entities[index].scale.y = scale.y;
    if (scale.z != NO_CHANGE) entities[index].scale.z = scale.z;
}

size_t EntityManager::size() const { 
    return entities.size(); 
}

Entity* EntityManager::getEntityAt(size_t index) {
    if (index < entities.size() && entities[index].active) {
        return &entities[index];
    }
    return nullptr;
}

void createEntity(std::string name, const std::vector<std::pair<float, std::vector<std::shared_ptr<Mesh>>>>& lodSpecs, glm::vec3 pos, glm::vec3 rotation, glm::vec3 scale, std::vector<int> cull_modes) {
    Entity entity;
    entity.name = name;
    entity.position = pos;
    entity.rotation = rotation;
    entity.scale = scale;
    entity.active = 1;
    
    extern unsigned int total_triangles;
    unsigned int total_mesh_triangles = 0;
    
    // Create LOD levels from the specs
    // Store materials from the first LOD to apply to corresponding meshes in other LODs
    std::vector<Material> baseMaterials;
    for (const auto& [maxDistance, meshes] : lodSpecs) {
        Entity::LODLevel level;
        level.maxDistance = maxDistance;
        level.meshes = meshes;
        
        // Save materials from first LOD, then apply them to all subsequent LODs
        if (entity.lod_levels.empty()) {
            // First LOD: save each mesh's material
            for (const auto& mesh : meshes) {
                if (mesh) {
                    baseMaterials.push_back(mesh->material);
                }
            }
        } else {
            // Subsequent LODs: apply corresponding LOD0 material to each mesh
            for (size_t i = 0; i < level.meshes.size(); ++i) {
                if (level.meshes[i] && i < baseMaterials.size()) {
                    level.meshes[i]->material = baseMaterials[i];
                }
            }
        }
        
        // Apply cull modes
        for (size_t i = 0; i < level.meshes.size(); ++i) {
            if (level.meshes[i]) {
                if (i < cull_modes.size() && cull_modes[i]) {
                    level.meshes[i]->cull_mode = cull_modes[i];
                } else {
                    level.meshes[i]->cull_mode = CULL_NONE;
                }
            }
        }
        
        // Count triangles only for the first (highest detail) LOD
        if (entity.lod_levels.empty()) {
            for (const auto& mesh : level.meshes) {
                if (mesh) {
                    total_mesh_triangles += mesh->TRIANGLE_COUNT;
                }
            }
        }
        
        entity.lod_levels.push_back(level);
    }
    
    total_triangles += total_mesh_triangles;
    
    printf("Created entity '%s' with %zu LOD levels (%u triangles)\n",
           name.c_str(), lodSpecs.size(), total_mesh_triangles);
    
    entity_manager.addEntity(std::move(entity));
}
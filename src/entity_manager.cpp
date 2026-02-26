#include "entity_manager.h"
#include "mesh_loader.h"
#include <cstdio>
#include <cmath>

// Global entity manager instance
EntityManager entity_manager;

size_t EntityManager::addEntity(Entity&& entity) {
    entities.push_back(std::move(entity));
    return entities.size() - 1;
}

std::optional<size_t> EntityManager::findEntity(std::string name) {
    for (size_t i = 0; i < entities.size(); ++i) {
        if (entities[i].active && entities[i].name == name) {
            return i;
        }
    }
    return std::nullopt;
}

bool EntityManager::updateEntity(std::string name, const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scale) {
    auto idxOpt = findEntity(name);
    if (!idxOpt.has_value()) return false;
    const size_t i = *idxOpt;

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
    
    unsigned int total_mesh_triangles = 0;
    
    // Create LOD levels from the specs
    for (const auto& [maxDistance, meshes] : lodSpecs) {
        Entity::LODLevel level;
        level.maxDistance = maxDistance;
        level.meshes = meshes;
        
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
        
    printf("Created entity '%s' with %zu LOD levels (%u triangles)\n",
           name.c_str(), lodSpecs.size(), total_mesh_triangles);
    
    entity_manager.addEntity(std::move(entity));
}

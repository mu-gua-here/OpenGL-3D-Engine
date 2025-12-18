#include "entity_manager.h"
#include <cstdio>
#include <cmath>

// Global entity manager instance
EntityManager entity_manager;

// Global triangle counter
extern unsigned int total_triangles;

size_t EntityManager::addEntity(Entity&& entity) {
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

    // Positions
    if (!isnan(pos.x)) entities[i].position.x = pos.x;
    if (!isnan(pos.y)) entities[i].position.y = pos.y;
    if (!isnan(pos.z)) entities[i].position.z = pos.z;
    
    // Rotations
    if (!isnan(rot.x)) entities[i].rotation.x = rot.x;
    if (!isnan(rot.y)) entities[i].rotation.y = rot.y;
    if (!isnan(rot.z)) entities[i].rotation.z = rot.z;
    
    // Scale
    if (!isnan(scale.x)) entities[i].scale.x = scale.x;
    if (!isnan(scale.y)) entities[i].scale.y = scale.y;
    if (!isnan(scale.z)) entities[i].scale.z = scale.z;

    return true;
}

void EntityManager::updateEntity(size_t index, const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scale) {        
    // Positions
    if (!isnan(pos.x)) entities[index].position.x = pos.x;
    if (!isnan(pos.y)) entities[index].position.y = pos.y;
    if (!isnan(pos.z)) entities[index].position.z = pos.z;
    
    // Rotations
    if (!isnan(rot.x)) entities[index].rotation.x = rot.x;
    if (!isnan(rot.y)) entities[index].rotation.y = rot.y;
    if (!isnan(rot.z)) entities[index].rotation.z = rot.z;
    
    // Scale
    if (!isnan(scale.x)) entities[index].scale.x = scale.x;
    if (!isnan(scale.y)) entities[index].scale.y = scale.y;
    if (!isnan(scale.z)) entities[index].scale.z = scale.z;
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

// Helper function to create entities with mesh loading and cull modes
void createEntity(std::string name, std::vector<std::unique_ptr<Mesh>>&& meshes, glm::vec3 pos, glm::vec3 rotation, glm::vec3 scale, std::vector<int> cull_modes) {
    Entity entity;
    entity.name = name;
    entity.position = pos;
    entity.rotation = rotation;
    entity.scale = scale;
    entity.active = 1;
    
    // Apply cull modes for individual submeshes
    for (size_t i = 0; i < meshes.size(); ++i) {
        if (meshes[i]) {
            if (i < cull_modes.size()) {
                // If specified use the given cull mode, otherwise use CULL_NONE
                if (cull_modes[i]) {
                    meshes[i]->cull_mode = cull_modes[i];
                } else {
                    meshes[i]->cull_mode = CULL_NONE;
                }
            }
        }
    }
    
    // Count triangles then move memory address
    unsigned int mesh_triangles = 0;
    for (const auto& mesh : meshes) {
        if (mesh) {
            mesh_triangles += mesh->TRIANGLE_COUNT;
        }
    }
    total_triangles += mesh_triangles;
    
    printf("Created entity '%s' with %zu submeshes (triangles: %u)\n",
           name.c_str(), meshes.size(), mesh_triangles);
    
    entity.meshes = std::move(meshes);
    entity_manager.addEntity(std::move(entity));
}

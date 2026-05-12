#pragma once

#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include "mesh.h"
struct Entity {
    // Basic properties
    std::string name;
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale;
    std::vector<std::shared_ptr<Mesh>> meshes;
    int active;

    // Physics
    int physics_body_index = -1;

    // Dynamic LOD system
    struct LODLevel {
        std::vector<std::shared_ptr<Mesh>> meshes;
        float maxDistance;  // Use this LOD up to this distance
    };
    
    std::vector<LODLevel> lod_levels;

    // Helper function to get appropriate LOD based on distance
    const std::vector<std::shared_ptr<Mesh>>& getMeshesForDistance(float distance) const {
        // Find the appropriate LOD level
        for (const auto& level : lod_levels) {
            if (distance <= level.maxDistance) {
                return level.meshes;
            }
        }
        
        // If beyond all LOD ranges, use the lowest detail (last one)
        if (!lod_levels.empty()) {
            return lod_levels.back().meshes;
        }
        
        // Fallback to empty (shouldn't happen)
        static std::vector<std::shared_ptr<Mesh>> empty;
        return empty;
    }
    
    glm::mat4 getModelMatrix(Entity* entity) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, entity->position);
        // Apply rotation using quaternion
        model = model * glm::mat4_cast(entity->rotation);
        // Scale last (applied first in vertex transformation: scale -> rotate -> translate)
        model = glm::scale(model, entity->scale);
        return model;
    }
};

class EntityManager {
private:
    std::vector<Entity> entities;
    
public:
    size_t addEntity(Entity&& entity);
    std::optional<size_t> findEntity(std::string name);
    bool updateEntity(std::string name, const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scale);
    void updateEntity(size_t index, const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scale);
    size_t size() const;
    Entity* getEntityAt(size_t index);
    
    template <typename Pred>
    void removeEntities(Pred&& pred);
};

extern EntityManager entity_manager;

void createEntity(std::string name, const std::vector<std::pair<float, std::vector<std::shared_ptr<Mesh>>>>& lodSpecs, glm::vec3 pos, glm::vec3 rotation, glm::vec3 scale, std::vector<int> cull_modes);

// Template implementation must be in header
template <typename Pred>
void EntityManager::removeEntities(Pred&& pred) {
    for (Entity& entity : entities) {
        if (entity.active && pred(entity)) {
            entity.meshes.clear();
            entity.active = false;
        }
    }
}

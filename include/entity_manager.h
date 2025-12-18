#pragma once

#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "mesh.h"

struct Entity {
    std::string name;
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
    std::vector<std::unique_ptr<Mesh>> meshes;
    int active;
};

class EntityManager {
private:
    std::vector<Entity> entities;
    
public:
    size_t addEntity(Entity&& entity);
    int findEntity(std::string name);
    bool updateEntity(std::string name, const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scale);
    void updateEntity(size_t index, const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scale);
    size_t size() const;
    Entity* getEntityAt(size_t index);
    
    template <typename Pred>
    void removeEntities(Pred&& pred);
};

extern EntityManager entity_manager;

// Helper function to create entities with mesh loading and cull modes
void createEntity(std::string name, std::vector<std::unique_ptr<Mesh>>&& meshes, glm::vec3 pos, glm::vec3 rotation, glm::vec3 scale, std::vector<int> cull_modes);

// Template implementation must be in header
template <typename Pred>
void EntityManager::removeEntities(Pred&& pred) {
    extern unsigned int total_triangles;
    for (Entity& entity : entities) {
        if (entity.active && pred(entity)) {
            for (const auto& mesh : entity.meshes) total_triangles -= mesh->TRIANGLE_COUNT;
            entity.meshes.clear();
            entity.active = false;
        }
    }
}
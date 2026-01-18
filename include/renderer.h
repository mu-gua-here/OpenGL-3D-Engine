#pragma once

#include <memory>
#include "shader.h"
#include "light.h"
#include "entity_manager.h"
#include "camera.h"

// Forward declarations
class Mesh;
struct Entity;

class Renderer {
private:
    std::unique_ptr<Shader> pbr_shader;
    std::unique_ptr<Shader> shadow_shader;
    std::unique_ptr<Shader> unlit_shader;
    std::unique_ptr<Shader> depth_prepass_shader;
    std::vector<Entity*> visibleEntities;  // Cache culled entities

    void bindMaterial(const Material* material);
    void renderInstancedMesh(Mesh* mesh, const std::vector<glm::mat4>& matrices);
    void drawMesh(Mesh* mesh, const glm::mat4& model);
    
public:
    Renderer();
    ~Renderer() = default;

    // Debug stats
    struct RenderStats {
        int entitiesTotal = 0;
        int entitiesCulled = 0;
        int entitiesRendered = 0;
        int drawCalls = 0;
        int instancedDrawCalls = 0;
        int instancesRendered = 0;
        int materialChanges = 0;
        int trianglesRendered = 0;
        
        void reset() {
            entitiesTotal = 0;
            entitiesCulled = 0;
            entitiesRendered = 0;
            drawCalls = 0;
            instancedDrawCalls = 0;
            instancesRendered = 0;
            materialChanges = 0;
            trianglesRendered = 0;
        }
    };
    
    RenderStats stats;
    
    void cullEntities(EntityManager& entity_manager, const glm::mat4& viewProj);
    void renderDepthPrepass();
    void renderShadowPass(EntityManager& entity_manager, const Light& light);
    void setGlobalUniforms(const Camera& camera, int shadowLightIndex);
    void drawUnlitMesh(Entity* entity, Mesh* mesh, const glm::vec3& color, int intensity);
    void renderScene(EntityManager& entity_manager);
};

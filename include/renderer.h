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
    
public:
    Renderer();
    ~Renderer() = default;
    
    void renderShadowPass(EntityManager& entity_manager, const Light& light);
    void setGlobalUniforms(const Camera& camera);
    void drawEntity(Entity* entity, const Camera& camera, int shadowLightIndex);
    void drawMesh(Entity* entity, Mesh* mesh, const Camera& camera, int shadowLightIndex);
    void drawUnlitMesh(Entity* entity, Mesh* mesh, const glm::vec3& color, int intensity);
};

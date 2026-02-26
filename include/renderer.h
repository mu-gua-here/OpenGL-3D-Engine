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
    std::unique_ptr<Shader> bloom_bright_shader;
    std::unique_ptr<Shader> bloom_blur_h_shader;
    std::unique_ptr<Shader> bloom_blur_v_shader;
    std::unique_ptr<Shader> bloom_composite_shader;
    
    std::vector<Entity*> visibleEntities;  // Cache culled entities

    void bindMaterial(const Material* material);
    void renderInstancedMesh(Mesh* mesh, const std::vector<glm::mat4>& matrices);
    void renderMesh(Mesh* mesh, const glm::mat4& model);
    
public:
    Renderer();
    ~Renderer() = default;

    enum RenderDetail {
        LOW = 0,
        MEDIUM = 1,
        HIGH = 2
    };

    int renderDetailLevel = HIGH;

    bool depthPrepassEnabled = true;
    bool vsyncEnabled = true;

    // MSAA buffers
    GLuint msaaFBO = 0;          // Framebuffer to store MSAA pixel data
    GLuint msaaColorBuffer = 0;  // Color buffer for MSAA
    GLuint msaaDepthBuffer = 0;  // Depth buffer for MSAA

    // Post-process framebuffer for bloom
    GLuint resolveFBO = 0;      // Framebuffer for capturing rendered scene
    
    // Bloom textures (screen-space post-process)
    bool bloomEnabled = true;
    GLuint screenColorTexture = 0;  // Copy of rendered screen
    GLuint bloomFBO = 0;            // FBO for bloom processing
    GLuint bloomBrightTexture = 0;  // Bright pass output
    GLuint bloomBlurH = 0;          // Horizontal blur output
    GLuint bloomBlurV = 0;          // Vertical blur output (final)
    GLuint screenQuadVAO = 0;
    GLuint screenQuadVBO = 0;
    int lastScreenWidth = 0;
    int lastScreenHeight = 0;
    float bloomThreshold = 0.7f;  // Lower threshold for more visible bloom
    float bloomIntensity = 0.6f;  // Higher intensity for more noticeable effect

    // Fog color
    bool fogEnabled = true;
    float fogStart = 40.0f;
    float fogEnd = 180.0f;
    glm::vec3 fogColor = glm::vec3(0.5f, 0.65f, 0.75f);

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
    void renderUnlitMesh(Mesh* mesh, const std::vector<glm::mat4>& matrices, const glm::vec3& color, int intensity);
    void renderPBRMeshes(const Camera& camera, int shadowLightIndex, EntityManager& entity_manager);
    void renderUnlitMeshes(EntityManager& entity_manager);
    void applyBloomPostProcess(int screenWidth, int screenHeight);
    void setBloomThreshold(float t) { bloomThreshold = t; }
    void setBloomIntensity(float i) { bloomIntensity = i; }
};
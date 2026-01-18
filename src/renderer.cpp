#include "renderer.h"
#include "shader.h"
#include "camera.h"
#include "entity_manager.h"
#include "mesh.h"
#include "light.h"
#include "shader_loading.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstdio>
#include <unordered_map>
#include <algorithm>

// Extern declarations
extern glm::mat4 view;
extern glm::mat4 projection;
extern glm::mat4 lightSpaceMatrix;
extern GLuint shadowMapFBO;
extern GLuint shadowMapTexture;
extern unsigned int SHADOW_WIDTH;
extern unsigned int SHADOW_HEIGHT;
extern GLuint default_texture_id;
extern Camera global_camera;
extern std::vector<Light> lights;

std::string buildAssetPath(const std::string& relative_path);

// Material batching hash functions
struct MaterialHash {
    size_t operator()(const Material* mat) const {
        size_t h1 = std::hash<GLuint>{}(mat->albedo_map);
        size_t h2 = std::hash<GLuint>{}(mat->normal_map);
        size_t h3 = std::hash<GLuint>{}(mat->orm_map);
        size_t h4 = std::hash<GLuint>{}(mat->emissive_map);
        return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
    }
};

struct MaterialEqual {
    bool operator()(const Material* a, const Material* b) const {
        return a->albedo_map == b->albedo_map &&
               a->normal_map == b->normal_map &&
               a->orm_map == b->orm_map &&
               a->emissive_map == b->emissive_map &&
               std::abs(a->metallic - b->metallic) < 0.01f &&
               std::abs(a->roughness - b->roughness) < 0.01f;
    }
};

// Frustum culling
struct Frustum {
    glm::vec4 planes[6];
    
    void extractFromMatrix(const glm::mat4& vp) {
        planes[0] = glm::vec4(vp[0][3] + vp[0][0], vp[1][3] + vp[1][0], vp[2][3] + vp[2][0], vp[3][3] + vp[3][0]);
        planes[1] = glm::vec4(vp[0][3] - vp[0][0], vp[1][3] - vp[1][0], vp[2][3] - vp[2][0], vp[3][3] - vp[3][0]);
        planes[2] = glm::vec4(vp[0][3] + vp[0][1], vp[1][3] + vp[1][1], vp[2][3] + vp[2][1], vp[3][3] + vp[3][1]);
        planes[3] = glm::vec4(vp[0][3] - vp[0][1], vp[1][3] - vp[1][1], vp[2][3] - vp[2][1], vp[3][3] - vp[3][1]);
        planes[4] = glm::vec4(vp[0][3] + vp[0][2], vp[1][3] + vp[1][2], vp[2][3] + vp[2][2], vp[3][3] + vp[3][2]);
        planes[5] = glm::vec4(vp[0][3] - vp[0][2], vp[1][3] - vp[1][2], vp[2][3] - vp[2][2], vp[3][3] - vp[3][2]);
        
        for (int i = 0; i < 6; i++) {
            float len = glm::length(glm::vec3(planes[i]));
            planes[i] /= len;
        }
    }
    
    bool sphereInFrustum(const glm::vec3& center, float radius) const {
        for (int i = 0; i < 6; i++) {
            if (glm::dot(glm::vec3(planes[i]), center) + planes[i].w < -radius) 
                return false;
        }
        return true;
    }
};

Renderer::Renderer() {
    try {
        std::string pbr_vert = loadShaderFile(buildAssetPath("res/shaders/pbr.vs"));
        std::string pbr_frag = loadShaderFile(buildAssetPath("res/shaders/pbr.fs"));
        std::string shadow_vert = loadShaderFile(buildAssetPath("res/shaders/shadow.vs"));
        std::string shadow_frag = loadShaderFile(buildAssetPath("res/shaders/shadow.fs"));
        std::string unlit_vert = loadShaderFile(buildAssetPath("res/shaders/unlit.vs"));
        std::string unlit_frag = loadShaderFile(buildAssetPath("res/shaders/unlit.fs"));
        std::string prepass_vert = loadShaderFile(buildAssetPath("res/shaders/depth_prepass.vs"));
        std::string prepass_frag = loadShaderFile(buildAssetPath("res/shaders/depth_prepass.fs"));

        pbr_shader = std::make_unique<Shader>(pbr_vert, pbr_frag);
        shadow_shader = std::make_unique<Shader>(shadow_vert, shadow_frag);
        unlit_shader = std::make_unique<Shader>(unlit_vert, unlit_frag);
        depth_prepass_shader =  std::make_unique<Shader>(prepass_vert, prepass_frag);
        printf("Shaders created successfully. Main: %u, Shadow: %u, Unlit: %u, Prepass: %u\n",
               pbr_shader->getProgram(), shadow_shader->getProgram(), unlit_shader->getProgram(), depth_prepass_shader->getProgram());
    } catch (const std::exception& e) {
        printf("Failed to create shaders: %s\n", e.what());
        throw;
    }
}

void Renderer::cullEntities(EntityManager& entity_manager, const glm::mat4& viewProj) {
    visibleEntities.clear();
    
    Frustum frustum;
    frustum.extractFromMatrix(viewProj);
    
    for (size_t i = 0; i < entity_manager.size(); i++) {
        Entity* entity = entity_manager.getEntityAt(i);
        if (!entity || !entity->active) continue;
        
        // Skip lights
        bool is_light = false;
        for (const auto& light : lights) {
            if (entity->name == light.entity_name) {
                is_light = true;
                break;
            }
        }
        if (is_light) continue;
        
        // Frustum cull
        float radius = glm::length(entity->scale) * 5.0f;
        if (frustum.sphereInFrustum(entity->position, radius)) {
            visibleEntities.push_back(entity);
        }
    }
}   

void Renderer::renderDepthPrepass() {
    // Disable color writes, only write depth
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE); 
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE); // Disable color
    glDepthFunc(GL_LESS);
    
    depth_prepass_shader->use();
    depth_prepass_shader->setMat4("view", view);
    depth_prepass_shader->setMat4("projection", projection);
    
    // Frustum culling
    Frustum frustum;
    frustum.extractFromMatrix(projection * view);
    
    // Batch all opaque geometry
    std::unordered_map<Mesh*, std::vector<glm::mat4>> depthBatches;
    
    for (Entity* entity : visibleEntities) {
        glm::mat4 model = entity->getModelMatrix(entity);
        float distance = glm::length(global_camera.position - entity->position);
        
        const auto& meshesToUse = entity->getMeshesForDistance(distance);
        for (auto& meshPtr : meshesToUse) {
            if (meshPtr && meshPtr->isValid()) {
                depthBatches[meshPtr.get()].push_back(model);
            }
        }
    }
    
    // Render depth-only
    int lastCullMode = -1;
    
    for (auto& [mesh, matrices] : depthBatches) {
        if (matrices.empty() || matrices.size() > mesh->maxInstances) continue;
        
        // Set cull mode
        if (mesh->cull_mode != lastCullMode) {
            switch (mesh->cull_mode) {
                case CULL_NONE: glDisable(GL_CULL_FACE); break;
                case CULL_BACK: glEnable(GL_CULL_FACE); glCullFace(GL_BACK); break;
                case CULL_FRONT: glEnable(GL_CULL_FACE); glCullFace(GL_FRONT); break;
            }
            lastCullMode = mesh->cull_mode;
        }
        
        // Bind albedo for alpha testing
        if (mesh->material.hasAlbedoMap()) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mesh->material.albedo_map);
            depth_prepass_shader->setInt("albedoMap", 0);
            depth_prepass_shader->setInt("hasAlbedoMap", 1);
        } else {
            depth_prepass_shader->setInt("hasAlbedoMap", 0);
        }
        
        // Upload instances
        glBindBuffer(GL_ARRAY_BUFFER, mesh->instanceVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, 
                        matrices.size() * sizeof(glm::mat4), 
                        matrices.data());
        
        glBindVertexArray(mesh->VAO);
        glDrawElementsInstanced(GL_TRIANGLES, mesh->INDEX_COUNT, GL_UNSIGNED_INT, 0, matrices.size());
    }
    glBindVertexArray(0);
}

void Renderer::renderShadowPass(EntityManager& entity_manager, const Light& light) {
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    
    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    
    glm::mat4 lightProjection, lightView;
    
    if (light.type == SPOT_LIGHT) {
        glm::vec3 lightTarget = light.position + light.direction;
        glm::vec3 up = glm::abs(light.direction.y) > 0.99f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
        lightView = glm::lookAt(light.position, lightTarget, up);
        float outerAngle = glm::degrees(glm::acos(light.outer_cutoff_cos));
        lightProjection = glm::perspective(glm::radians(outerAngle * 2.0f), 1.0f, 0.5f, 100.0f);
    } else if (light.type == POINT_LIGHT) {
        glm::vec3 finalDir = glm::normalize(light.position);
        glm::vec3 up = glm::abs(finalDir.y) > 0.99f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
        lightProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.5f, 100.0f);
        lightView = glm::lookAt(light.position, glm::vec3(0), up);
    } else if (light.type == DIR_LIGHT) {
        glm::vec3 finalDir = glm::normalize(light.direction);
        glm::vec3 up = std::abs(finalDir.y) > 0.999f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
        glm::mat4 shadowFocusProj = glm::perspective(glm::radians(global_camera.fov),
                                                    global_camera.aspect_ratio, 0.1f, 50.0f);
        glm::mat4 invVP = glm::inverse(shadowFocusProj * view);
        
        std::vector<glm::vec3> corners;
        glm::vec3 center(0.0f);
        for (unsigned int i = 0; i < 8; ++i) {
            glm::vec4 pt = invVP * glm::vec4((i&1)?1:-1, (i&2)?1:-1, (i&4)?1:-1, 1.0f);
            glm::vec3 c = glm::vec3(pt / pt.w);
            corners.push_back(c);
            center += c;
        }
        center /= 8.0f;

        float radius = 0.0f;
        for (const auto& v : corners) radius = std::max(radius, glm::length(v - center));
        radius = std::ceil(radius * 16.0f) / 16.0f;

        lightView = glm::lookAt(center - finalDir * radius, center, up);
        lightProjection = glm::ortho(-radius, radius, -radius, radius, -radius * 5.0f, radius * 5.0f);

        glm::mat4 tempShadowMatrix = lightProjection * lightView;
        glm::vec4 shadowOrigin = tempShadowMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        shadowOrigin = shadowOrigin * (float)SHADOW_WIDTH / 2.0f;
        glm::vec4 roundedOrigin = glm::round(shadowOrigin);
        glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
        roundOffset = roundOffset * 2.0f / (float)SHADOW_WIDTH;
        roundOffset.z = 0.0f; roundOffset.w = 0.0f;
        lightProjection[3] += roundOffset;
    }

    lightSpaceMatrix = lightProjection * lightView;
    shadow_shader->use();
    shadow_shader->setMat4("lightSpaceMatrix", lightSpaceMatrix);

    Frustum frustum;
    frustum.extractFromMatrix(projection * view);

    // Batch shadow rendering by mesh
    std::unordered_map<Mesh*, std::vector<glm::mat4>> shadowBatches;
    
    for (size_t i = 0; i < entity_manager.size(); i++) {
        Entity* entity = entity_manager.getEntityAt(i);
        if (!entity || !entity->active) continue;
        
        bool is_light_entity = false;
        for (const auto& l : lights) {
            if (entity->name == l.entity_name) {
                is_light_entity = true;
                break;
            }
        }
        if (is_light_entity) continue;

        float radius = glm::length(entity->scale) * 5.0f;
        if (!frustum.sphereInFrustum(entity->position, radius)) {
            continue;  // Skip shadow rendering for this entity
        }

        glm::mat4 model = entity->getModelMatrix(entity);
        for (const auto& mesh : entity->meshes) {
            if (mesh && mesh->isValid()) {
                shadowBatches[mesh.get()].push_back(model);
            }
        }
    }

    // Render shadow batches with minimal state changes
    GLuint lastTexture = 0;
    int lastCullMode = -1;
    
    for (auto& [mesh, matrices] : shadowBatches) {
        if (matrices.empty()) continue;
        
        // Only change cull mode if different
        if (mesh->cull_mode != lastCullMode) {
            if (mesh->cull_mode == CULL_NONE) {
                glDisable(GL_CULL_FACE);
            } else {
                glEnable(GL_CULL_FACE);
                glCullFace(GL_FRONT);
            }
            lastCullMode = mesh->cull_mode;
        }

        // Only bind texture if different
        GLuint texture_to_use = mesh->material.hasAlbedoMap() ? mesh->material.albedo_map : default_texture_id;
        if (texture_to_use != lastTexture) {
            glBindTexture(GL_TEXTURE_2D, texture_to_use);
            lastTexture = texture_to_use;
        }

        glBindBuffer(GL_ARRAY_BUFFER, mesh->instanceVBO);
        glBufferData(GL_ARRAY_BUFFER, matrices.size() * sizeof(glm::mat4), matrices.data(), GL_DYNAMIC_DRAW);

        glBindVertexArray(mesh->VAO);
        glDrawElementsInstanced(GL_TRIANGLES, mesh->INDEX_COUNT, GL_UNSIGNED_INT, 0, matrices.size());
    }

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glCullFace(GL_BACK);
}

void Renderer::setGlobalUniforms(const Camera& camera, int shadowLightIndex) {
    std::vector<glm::vec3> positions, colors, directions;
    std::vector<float> intensities, inner_cutoffs, outer_cutoffs, types;

    for (const auto& light : lights) {
        positions.push_back(light.position);
        colors.push_back(light.color);
        intensities.push_back(light.intensity);
        directions.push_back(light.direction);
        inner_cutoffs.push_back(light.inner_cutoff_cos);
        outer_cutoffs.push_back(light.outer_cutoff_cos);
        types.push_back((float)light.type);
    }

    pbr_shader->use();
    pbr_shader->setInt("lightCount", (int)lights.size());
    pbr_shader->setVec3Array("lightPositions", positions);
    pbr_shader->setVec3Array("lightColors", colors);
    pbr_shader->setFloatArray("lightIntensities", intensities);
    pbr_shader->setVec3Array("lightDirections", directions);
    pbr_shader->setFloatArray("lightInnerCutoffs", inner_cutoffs);
    pbr_shader->setFloatArray("lightOuterCutoffs", outer_cutoffs);
    pbr_shader->setFloatArray("lightTypes", types);
    pbr_shader->setMat4("view", view);
    pbr_shader->setMat4("projection", projection);
    pbr_shader->setMat4("lightSpaceMatrix", lightSpaceMatrix);
    pbr_shader->setVec3("viewPos", camera.position);
    pbr_shader->setInt("shadowLightIndex", shadowLightIndex);
}

void Renderer::bindMaterial(const Material* material) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, material->hasAlbedoMap() ? material->albedo_map : default_texture_id);
    pbr_shader->setInt("albedoMap", 0);
    pbr_shader->setInt("hasAlbedoMap", material->hasAlbedoMap() ? 1 : 0);
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, material->hasNormalMap() ? material->normal_map : default_texture_id);
    pbr_shader->setInt("normalMap", 1);
    pbr_shader->setInt("hasNormalMap", material->hasNormalMap() ? 1 : 0);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, material->hasORMMap() ? material->orm_map : default_texture_id);
    pbr_shader->setInt("ormMap", 2);
    pbr_shader->setInt("hasORMMap", material->hasORMMap() ? 1 : 0);
    pbr_shader->setInt("hasHeightMap", material->hasHeightMap() ? 1 : 0);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, material->hasEmissiveMap() ? material->emissive_map : default_texture_id);
    pbr_shader->setInt("emissiveMap", 3);
    pbr_shader->setInt("hasEmissiveMap", material->hasEmissiveMap() ? 1 : 0);

    // Ensure shadow map stays bound at texture unit 4
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, shadowMapTexture);
    pbr_shader->setInt("shadowMap", 4);

    // Set material properties
    pbr_shader->setVec3("baseColor", material->base_color);
    pbr_shader->setFloat("metallic", material->metallic);
    pbr_shader->setFloat("roughness", material->roughness);
    pbr_shader->setFloat("ao", material->ao);
    pbr_shader->setVec3("emissive", material->emissive);
    pbr_shader->setFloat("heightScale", material->height_scale);
}

void Renderer::renderScene(EntityManager& entity_manager) {
    stats.reset();  // Reset at start of frame
    
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthFunc(GL_EQUAL);
    glDepthMask(GL_FALSE);

    Frustum frustum;
    frustum.extractFromMatrix(projection * view);
    
    std::vector<std::pair<float, std::pair<Mesh*, glm::mat4>>> transparentObjects;
    std::unordered_map<const Material*, std::unordered_map<Mesh*, std::vector<glm::mat4>>> materialBatches;
    
    for (size_t i = 0; i < entity_manager.size(); i++) {
        Entity* entity = entity_manager.getEntityAt(i);
        if (!entity || !entity->active) continue;
        
        stats.entitiesTotal++;  // COUNT TOTAL
        
        bool is_light = false;
        for (const auto& light : lights) {
            if (entity->name == light.entity_name) {
                is_light = true;
                break;
            }
        }
        if (is_light) continue;
        
        float radius = glm::length(entity->scale) * 5.0f;
        if (!frustum.sphereInFrustum(entity->position, radius)) {
            stats.entitiesCulled++;  // COUNT CULLED
            continue;
        }
        
        stats.entitiesRendered++;  // COUNT RENDERED
        
        glm::mat4 model = entity->getModelMatrix(entity);
        
        // Calculate distance for LOD selection
        float distance = glm::length(global_camera.position - entity->position);
        const auto& meshesToUse = entity->getMeshesForDistance(distance);  // USE LOD HERE!
        
        for (auto& meshPtr : meshesToUse) {  // Use LOD meshes
            if (meshPtr && meshPtr->isValid()) {
                if (meshPtr->material.alphaMode == BLEND) {
                    transparentObjects.push_back({distance, {meshPtr.get(), model}});
                } else {
                    materialBatches[&meshPtr->material][meshPtr.get()].push_back(model);
                }
            }
        }
    }
    
    int lastCullMode = -1;
    
    for (auto& [material, meshBatches] : materialBatches) {
        bindMaterial(material);
        stats.materialChanges++;  // COUNT MATERIAL CHANGES
        
        for (auto& [mesh, matrices] : meshBatches) {
            if (mesh->cull_mode != lastCullMode) {
                switch (mesh->cull_mode) {
                    case CULL_NONE: glDisable(GL_CULL_FACE); break;
                    case CULL_BACK: glEnable(GL_CULL_FACE); glCullFace(GL_BACK); break;
                    case CULL_FRONT: glEnable(GL_CULL_FACE); glCullFace(GL_FRONT); break;
                }
                lastCullMode = mesh->cull_mode;
            }
            
            if (matrices.size() == 1) {                
                glBindVertexArray(mesh->VAO);
                glDrawElements(GL_TRIANGLES, mesh->INDEX_COUNT, GL_UNSIGNED_INT, 0);
                
                stats.drawCalls++;  // COUNT DRAW CALL
                stats.trianglesRendered += mesh->TRIANGLE_COUNT;
            } else {
                renderInstancedMesh(mesh, matrices);
                
                stats.instancedDrawCalls++;  // COUNT INSTANCED CALL
                stats.instancesRendered += matrices.size();
                stats.trianglesRendered += mesh->TRIANGLE_COUNT * matrices.size();
            }
        }
    }
    
    glBindVertexArray(0);
    
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);

    std::sort(transparentObjects.begin(), transparentObjects.end(), 
              [](const auto& a, const auto& b) {
        return a.first > b.first;
    });

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (auto& item : transparentObjects) {
        bindMaterial(&item.second.first->material);
        stats.materialChanges++;
        drawMesh(item.second.first, item.second.second);
        stats.drawCalls++;
        stats.trianglesRendered += item.second.first->TRIANGLE_COUNT;
    }

    glDisable(GL_BLEND);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

void Renderer::renderInstancedMesh(Mesh* mesh, const std::vector<glm::mat4>& matrices) {
    if (matrices.empty() || matrices.size() > mesh->maxInstances) return;
    
    // Upload instance matrices
    glBindBuffer(GL_ARRAY_BUFFER, mesh->instanceVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, 
                    matrices.size() * sizeof(glm::mat4), 
                    matrices.data());
    
    // Draw instanced
    glBindVertexArray(mesh->VAO);
    glDrawElementsInstanced(GL_TRIANGLES, mesh->INDEX_COUNT, 
                           GL_UNSIGNED_INT, 0, matrices.size());
    glBindVertexArray(0);
}

void Renderer::drawMesh(Mesh* mesh, const glm::mat4& model) {
    if (mesh->TRIANGLE_COUNT == 0 || !mesh->isValid()) return;

    // Handle culling
    switch (mesh->cull_mode) {
        case CULL_NONE: glDisable(GL_CULL_FACE); break;
        case CULL_BACK: glEnable(GL_CULL_FACE); glCullFace(GL_BACK); break;
        case CULL_FRONT: glEnable(GL_CULL_FACE); glCullFace(GL_FRONT); break;
    }

    // Calculate matrices
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));

    // Set mesh-specific uniforms ONLY
    pbr_shader->setMat4("model", model);
    pbr_shader->setMat3("normalMatrix", normalMatrix);

    // Draw
    glBindVertexArray(mesh->VAO);
    glDrawElements(GL_TRIANGLES, mesh->INDEX_COUNT, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void Renderer::drawUnlitMesh(Entity* entity, Mesh* mesh, const glm::vec3& color, int intensity) {
    if (!entity->active || mesh->TRIANGLE_COUNT == 0 || !mesh->isValid()) return;
    
    glDisable(GL_CULL_FACE);
    unlit_shader->use();
    
    glm::mat4 model = glm::translate(glm::mat4(1.0f), entity->position) *
                      glm::rotate(glm::mat4(1.0f), glm::radians(entity->rotation.z), glm::vec3(0, 0, 1)) *
                      glm::rotate(glm::mat4(1.0f), glm::radians(entity->rotation.y), glm::vec3(0, 1, 0)) *
                      glm::rotate(glm::mat4(1.0f), glm::radians(entity->rotation.x), glm::vec3(1, 0, 0)) *
                      glm::scale(glm::mat4(1.0f), entity->scale);
    
    unlit_shader->setMat4("model", model);
    unlit_shader->setMat4("view", view);
    unlit_shader->setMat4("projection", projection);
    unlit_shader->setVec3("emissiveColor", color);
    unlit_shader->setFloat("emissiveIntensity", intensity);
    
    glBindVertexArray(mesh->VAO);
    glDrawElements(GL_TRIANGLES, mesh->INDEX_COUNT, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);
}
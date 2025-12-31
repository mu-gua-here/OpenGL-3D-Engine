#include "renderer.h"
#include "shader.h"
#include "camera.h"
#include "entity_manager.h"
#include "mesh.h"
#include "light.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstdio>

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

std::string loadShaderFile(const std::string& path);
std::string buildAssetPath(const std::string& relative_path);

Renderer::Renderer() {
    try {
        // Load shaders from files instead of hardcoded strings
        std::string pbr_vert = loadShaderFile(buildAssetPath("res/shaders/pbr.vs"));
        std::string pbr_frag = loadShaderFile(buildAssetPath("res/shaders/pbr.fs"));
        std::string shadow_vert = loadShaderFile(buildAssetPath("res/shaders/shadow.vs"));
        std::string shadow_frag = loadShaderFile(buildAssetPath("res/shaders/shadow.fs"));
        std::string unlit_vert = loadShaderFile(buildAssetPath("res/shaders/unlit.vs"));
        std::string unlit_frag = loadShaderFile(buildAssetPath("res/shaders/unlit.fs"));
        
        pbr_shader = std::make_unique<Shader>(pbr_vert, pbr_frag);
        shadow_shader = std::make_unique<Shader>(shadow_vert, shadow_frag);
        unlit_shader = std::make_unique<Shader>(unlit_vert, unlit_frag);
        printf("Shaders created successfully. Main: %u, Shadow: %u, Unlit: %u\n",
               pbr_shader->getProgram(), shadow_shader->getProgram(), unlit_shader->getProgram());
    } catch (const std::exception& e) {
        printf("Failed to create shaders: %s\n", e.what());
        throw;
    }
}

void Renderer::renderShadowPass(EntityManager& entity_manager, const Light& light) {
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    
    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    
    glm::mat4 lightProjection;
    glm::mat4 lightView;
    
    if (light.type == SPOT_LIGHT) {
        // Move along the light's actual direction vector
        glm::vec3 lightTarget = light.position + light.direction;
        glm::vec3 finalDir = light.direction;
        
        // Choose up vector dynamically based on the calculated light direction
        glm::vec3 up = glm::abs(finalDir.y) > 0.99f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
        
        lightView = glm::lookAt(light.position, lightTarget, up);
        float outerAngle = glm::degrees(glm::acos(light.outer_cutoff_cos));
        lightProjection = glm::perspective(glm::radians(outerAngle * 2.0f), 1.0f, 0.5f, 100.0f);
                    
    } else if (light.type == POINT_LIGHT) {
        // Omni-directional point light
        glm::vec3 finalDir = glm::normalize(light.position - glm::vec3(0));

        glm::vec3 up = glm::abs(finalDir.y) > 0.99f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
        
        lightProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.5f, 100.0f);
        lightView = glm::lookAt(light.position, glm::vec3(0), up);
                    
    } else if (light.type == DIR_LIGHT) {
        glm::vec3 finalDir = glm::normalize(light.direction);
        glm::vec3 up = std::abs(finalDir.y) > 0.999f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
        
        // Center shadow map on camera position (follow camera)
        glm::vec3 shadowCenter = global_camera.position;
        
        // Calculate light view matrix
        lightView = glm::lookAt(shadowCenter - finalDir * 150.0f, shadowCenter, up);
        
        // Orthographic bounds
        float orthoSize = 50.0f;
        lightProjection = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, 0.1f, 300.0f);
        
        glm::mat4 shadowMatrix = lightProjection * lightView;
        
        // Transform shadow center to light space
        glm::vec4 shadowOrigin = shadowMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        shadowOrigin = shadowOrigin / shadowOrigin.w;
        shadowOrigin = shadowOrigin * float(SHADOW_WIDTH) / 2.0f;
        
        // Round to nearest texel
        glm::vec4 roundedOrigin = glm::round(shadowOrigin);
        glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
        roundOffset = roundOffset * 2.0f / float(SHADOW_WIDTH);
        roundOffset.z = 0.0f;
        roundOffset.w = 0.0f;
        
        // Adjust projection matrix to snap to texels
        lightProjection[3] += roundOffset;
    }
    
    lightSpaceMatrix = lightProjection * lightView;

    shadow_shader->use();
    shadow_shader->setMat4("lightSpaceMatrix", lightSpaceMatrix);

    // Render all entities
    for (size_t i = 0; i < entity_manager.size(); i++) {
        Entity* entity = entity_manager.getEntityAt(i);
        if (entity && entity->active) {
            bool is_light_entity = false;
            for (const auto& l : lights) {
                if (entity->name == l.entity_name) {
                    is_light_entity = true;
                    break;
                }
            }
            if (is_light_entity) continue;

            for (const auto& mesh : entity->meshes) {
                if (mesh && mesh->isValid()) {
                    if (mesh->cull_mode == CULL_NONE) {
                        glDisable(GL_CULL_FACE);
                    } else {
                        glEnable(GL_CULL_FACE);
                        glCullFace(GL_FRONT);
                    }

                    glm::mat4 scale_matrix = glm::scale(glm::mat4(1.0f), entity->scale);
                    glm::mat4 rotation_x = glm::rotate(glm::mat4(1.0f), glm::radians(entity->rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
                    glm::mat4 rotation_y = glm::rotate(glm::mat4(1.0f), glm::radians(entity->rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
                    glm::mat4 rotation_z = glm::rotate(glm::mat4(1.0f), glm::radians(entity->rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
                    glm::mat4 rotation = rotation_z * rotation_y * rotation_x;
                    glm::mat4 translation = glm::translate(glm::mat4(1.0f), entity->position);
                    glm::mat4 model = translation * rotation * scale_matrix;

                    shadow_shader->setMat4("model", model);
                    
                    // Bind texture for shadow shaders to test alpha values in texture
                    GLuint texture_to_use = mesh->material.hasAlbedoMap() ? mesh->material.albedo_map : default_texture_id;
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, texture_to_use);
                    shadow_shader->setInt("u_texture", 0);
                    
                    glBindVertexArray(mesh->VAO);
                    glDrawElements(GL_TRIANGLES, mesh->INDEX_COUNT, GL_UNSIGNED_INT, 0);
                }
            }
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glCullFace(GL_BACK);
}

void Renderer::setGlobalUniforms(const Camera& camera) {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> colors;
    std::vector<float> intensities;
    std::vector<glm::vec3> directions;
    std::vector<float> inner_cutoffs;
    std::vector<float> outer_cutoffs;
    std::vector<float> types;

    for (const auto& light : lights) {
        positions.push_back(light.position);
        colors.push_back(light.color);
        intensities.push_back(light.intensity);
        directions.push_back(light.direction);
        inner_cutoffs.push_back(light.inner_cutoff_cos);
        outer_cutoffs.push_back(light.outer_cutoff_cos);
        types.push_back((float)light.type); // Send light type as float to match GLSL
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

    // Camera uniforms
    pbr_shader->setMat4("view", view);
    pbr_shader->setMat4("projection", projection);
    pbr_shader->setMat4("lightSpaceMatrix", lightSpaceMatrix);
    pbr_shader->setVec3("viewPos", camera.position);
}

void Renderer::drawEntity(Entity* entity, int shadowLightIndex) {
    if (!entity->active) return;
    for (const auto& meshPtr : entity->meshes) {
        Mesh* mesh = meshPtr.get();
        drawMesh(entity, mesh, shadowLightIndex);
    }
}

void Renderer::drawMesh(Entity* entity, Mesh* mesh, int shadowLightIndex) {
    if (!entity->active || mesh->TRIANGLE_COUNT == 0 || !mesh->isValid()) {
        return;
    }

    if (mesh->VAO == 0 || mesh->VBO == 0) {
        printf("Error: Mesh has an invalid VAO (%u) or VBO (%u).\n", mesh->VAO, mesh->VBO);
        return;
    }

    // Handle culling
    switch (mesh->cull_mode) {
        case CULL_NONE:
            glDisable(GL_CULL_FACE);
            break;
        case CULL_BACK:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            break;
        case CULL_FRONT:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            break;
    }

    // Calculate matrices
    glm::mat4 scale_matrix = glm::scale(glm::mat4(1.0f), entity->scale);
    glm::mat4 rotation_x = glm::rotate(glm::mat4(1.0f), glm::radians(entity->rotation.x),
                                       glm::vec3(1.0f, 0.0f, 0.0f));
    glm::mat4 rotation_y = glm::rotate(glm::mat4(1.0f), glm::radians(entity->rotation.y),
                                       glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 rotation_z = glm::rotate(glm::mat4(1.0f), glm::radians(entity->rotation.z),
                                       glm::vec3(0.0f, 0.0f, 1.0f));
    glm::mat4 rotation = rotation_z * rotation_y * rotation_x;
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), entity->position);
    glm::mat4 model = translation * rotation * scale_matrix;
    glm::mat3 normal = glm::transpose(glm::inverse(glm::mat3(model)));

    // Set mesh uniforms
    pbr_shader->setMat4("model", model);
    pbr_shader->setMat3("normalMatrix", normal);
    pbr_shader->setInt("shadowLightIndex", shadowLightIndex);

    pbr_shader->setMat4("lightSpaceMatrix", lightSpaceMatrix);

    // Set material properties
    pbr_shader->setVec3("baseColor", mesh->material.base_color);
    pbr_shader->setFloat("metallic", mesh->material.metallic);
    pbr_shader->setFloat("roughness", mesh->material.roughness);
    pbr_shader->setFloat("ao", mesh->material.ao);
    pbr_shader->setVec3("emissive", mesh->material.emissive);
    pbr_shader->setFloat("heightScale", mesh->material.height_scale);

    // Bind textures and set flags
    int texture_unit = 0;

    // Albedo map
    glActiveTexture(GL_TEXTURE0 + texture_unit);
    GLuint albedo_to_use = mesh->material.hasAlbedoMap() ? mesh->material.albedo_map : default_texture_id;
    glBindTexture(GL_TEXTURE_2D, albedo_to_use);
    pbr_shader->setInt("albedoMap", texture_unit);
    pbr_shader->setInt("hasAlbedoMap", mesh->material.hasAlbedoMap() ? 1 : 0);
    texture_unit++;
    
    // Normal map
    glActiveTexture(GL_TEXTURE0 + texture_unit);
    GLuint normal_to_use = mesh->material.hasNormalMap() ? mesh->material.normal_map : default_texture_id;
    glBindTexture(GL_TEXTURE_2D, normal_to_use);
    pbr_shader->setInt("normalMap", texture_unit);
    pbr_shader->setInt("hasNormalMap", mesh->material.hasNormalMap() ? 1 : 0);
    texture_unit++;

    // ORM map
    glActiveTexture(GL_TEXTURE0 + texture_unit);
    GLuint orm_to_use = mesh->material.hasORMMap() ? mesh->material.orm_map : default_texture_id;
    glBindTexture(GL_TEXTURE_2D, orm_to_use);
    pbr_shader->setInt("ormMap", texture_unit);
    pbr_shader->setInt("hasORMMap", mesh->material.hasORMMap() ? 1 : 0);
    pbr_shader->setInt("hasHeightMap", mesh->material.hasHeightMap() ? 1 : 0);
    texture_unit++;

    // Emissive map
    glActiveTexture(GL_TEXTURE0 + texture_unit);
    GLuint emissive_to_use = mesh->material.hasEmissiveMap() ? mesh->material.emissive_map : default_texture_id;
    glBindTexture(GL_TEXTURE_2D, emissive_to_use);
    pbr_shader->setInt("emissiveMap", texture_unit);
    pbr_shader->setInt("hasEmissiveMap", mesh->material.hasEmissiveMap() ? 1 : 0);
    texture_unit++;

    // Shadow map
    glActiveTexture(GL_TEXTURE0 + texture_unit);
    glBindTexture(GL_TEXTURE_2D, shadowMapTexture);
    pbr_shader->setInt("shadowMap", texture_unit);

    // Draw
    glBindVertexArray(mesh->VAO);
    glDrawElements(GL_TRIANGLES, mesh->INDEX_COUNT, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // Reset to texture unit 0
    glActiveTexture(GL_TEXTURE0);
}

void Renderer::drawUnlitMesh(Entity* entity, Mesh* mesh, const glm::vec3& color, int intensity) {
    if (!entity->active || mesh->TRIANGLE_COUNT == 0 || !mesh->isValid()) {
        return;
    }
    
    // Disable culling for light meshes
    glDisable(GL_CULL_FACE);
    
    unlit_shader->use();
    
    // Calculate matrices
    glm::mat4 scale_matrix = glm::scale(glm::mat4(1.0f), entity->scale);
    glm::mat4 rotation_x = glm::rotate(glm::mat4(1.0f), glm::radians(entity->rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::mat4 rotation_y = glm::rotate(glm::mat4(1.0f), glm::radians(entity->rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 rotation_z = glm::rotate(glm::mat4(1.0f), glm::radians(entity->rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    glm::mat4 rotation = rotation_z * rotation_y * rotation_x;
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), entity->position);
    glm::mat4 model = translation * rotation * scale_matrix;
    
    // Set uniforms
    unlit_shader->setMat4("model", model);
    unlit_shader->setMat4("view", view);
    unlit_shader->setMat4("projection", projection);
    unlit_shader->setVec3("emissiveColor", color);
    unlit_shader->setFloat("emissiveIntensity", intensity);
    
    // Draw
    glBindVertexArray(mesh->VAO);
    glDrawElements(GL_TRIANGLES, mesh->INDEX_COUNT, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    
    glEnable(GL_CULL_FACE);
}

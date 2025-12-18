#include "skybox.h"
#include "camera.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <string>
#include <cstdio>

// Forward declarations for external functions
extern GLuint loadCubemap(const char* faces[6]);
extern const float SKYBOX_VERTICES[];
extern std::string skybox_vertex_shader;
extern std::string skybox_fragment_shader;

void Skybox::bindSkybox(const char* faces[6]) {
    cubemap_texture.push_back(loadCubemap(faces));
    
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(SKYBOX_VERTICES), &SKYBOX_VERTICES, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindVertexArray(0);
}

void Skybox::initShader() {
    try {
        skybox_shader = std::make_unique<Shader>(skybox_vertex_shader.c_str(), skybox_fragment_shader.c_str());
        printf("Skybox shaders created successfully. ID: %u\n", skybox_shader->getProgram());
    } catch (const std::exception& e) {
        printf("Failed to create skybox shaders: %s\n", e.what());
        throw;
    }
}

void Skybox::render(Camera* camera) {
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    
    skybox_shader->use();
    
    // Get view matrix and remove translation
    glm::mat4 skybox_view = glm::mat4(glm::mat3(camera_get_view_matrix(camera)));
    glm::mat4 skybox_projection = camera_get_projection(camera);
    
    skybox_shader->setMat4("view", skybox_view);
    skybox_shader->setMat4("projection", skybox_projection);
    
    glBindVertexArray(VAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap_texture[0]);
    skybox_shader->setInt("skybox", 0);
    
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
}

void Skybox::cleanup() {
    if (VAO != 0) glDeleteVertexArrays(1, &VAO);
    if (VBO != 0) glDeleteBuffers(1, &VBO);
    for (GLuint tex : cubemap_texture) {
        if (tex != 0) glDeleteTextures(1, &tex);
    }
    cubemap_texture.clear();
    VAO = VBO = 0;
}
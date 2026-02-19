#include "bloom.h"
#include "shader.h"
#include "shader_loading.h"
#include <glm/glm.hpp>
#include <cstdio>

std::string buildAssetPath(const std::string& relative_path);

Bloom::Bloom(int width, int height) : width(width), height(height) {
    try {
        // Load shaders
        std::string bright_vert = loadShaderFile(buildAssetPath("res/shaders/bloom_bright.vs"));
        std::string bright_frag = loadShaderFile(buildAssetPath("res/shaders/bloom_bright.fs"));
        std::string blur_h_vert = loadShaderFile(buildAssetPath("res/shaders/bloom_blur_h.vs"));
        std::string blur_h_frag = loadShaderFile(buildAssetPath("res/shaders/bloom_blur_h.fs"));
        std::string blur_v_vert = loadShaderFile(buildAssetPath("res/shaders/bloom_blur_v.vs"));
        std::string blur_v_frag = loadShaderFile(buildAssetPath("res/shaders/bloom_blur_v.fs"));
        std::string composite_vert = loadShaderFile(buildAssetPath("res/shaders/bloom_composite.vs"));
        std::string composite_frag = loadShaderFile(buildAssetPath("res/shaders/bloom_composite.fs"));
        
        brightPassShader = std::make_unique<Shader>(bright_vert, bright_frag);
        blurShaderH = std::make_unique<Shader>(blur_h_vert, blur_h_frag);
        blurShaderV = std::make_unique<Shader>(blur_v_vert, blur_v_frag);
        
        printf("Bloom shaders created successfully\n");
        
        createFramebuffers();
        createScreenQuad();
        
    } catch (const std::exception& e) {
        printf("Failed to initialize bloom: %s\n", e.what());
        throw;
    }
}

Bloom::~Bloom() {
    cleanup();
}

void Bloom::init(int w, int h) {
    // Skip if size hasn't changed and framebuffers are already created
    if (width == w && height == h && brightPassFBO != 0) {
        return;  // Already initialized with this size
    }
    
    width = w;
    height = h;
    
    // Delete old framebuffers before creating new ones
    if (brightPassFBO) {
        glDeleteFramebuffers(1, &brightPassFBO);
        glDeleteTextures(1, &brightPassTexture);
        brightPassFBO = 0;
        brightPassTexture = 0;
    }
    
    for (int i = 0; i < 2; i++) {
        if (blurFBO[i]) {
            glDeleteFramebuffers(1, &blurFBO[i]);
            glDeleteTextures(1, &blurTexture[i]);
            blurFBO[i] = 0;
            blurTexture[i] = 0;
        }
    }
    createFramebuffers();
}

void Bloom::createFramebuffers() {
    // Create bright pass framebuffer
    glGenFramebuffers(1, &brightPassFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, brightPassFBO);
    
    glGenTextures(1, &brightPassTexture);
    glBindTexture(GL_TEXTURE_2D, brightPassTexture);
    // Use quarter resolution for bright pass
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width / 4, height / 4, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brightPassTexture, 0);
    
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        printf("ERROR: Bright pass framebuffer not complete\n");
    }
    
    // Create blur framebuffers (pingpong for horizontal and vertical blur)
    for (int i = 0; i < 2; i++) {
        glGenFramebuffers(1, &blurFBO[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, blurFBO[i]);
        
        glGenTextures(1, &blurTexture[i]);
        glBindTexture(GL_TEXTURE_2D, blurTexture[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width / 4, height / 4, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, blurTexture[i], 0);
        
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            printf("ERROR: Blur framebuffer %d not complete\n", i);
        }
    }
}

void Bloom::createScreenQuad() {
    float quadVertices[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
    };
    
    glGenVertexArrays(1, &screenQuadVAO);
    glGenBuffers(1, &screenQuadVBO);
    
    glBindVertexArray(screenQuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, screenQuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    
    glBindVertexArray(0);
}

GLuint Bloom::renderBloom(GLuint colorTexture) {
    // Bright pass - downsample to half resolution and extract bright pixels
    glBindFramebuffer(GL_FRAMEBUFFER, brightPassFBO);
    glViewport(0, 0, width / 2, height / 2);  // Render at half resolution
    glClear(GL_COLOR_BUFFER_BIT);
    
    brightPassShader->use();
    brightPassShader->setInt("colorTexture", 0);
    brightPassShader->setFloat("threshold", bloomThreshold);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    
    glBindVertexArray(screenQuadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    // Blur - further downsample and blur horizontally
    glBindFramebuffer(GL_FRAMEBUFFER, blurFBO[0]);
    glViewport(0, 0, width / 4, height / 4);  // Quarter resolution
    glClear(GL_COLOR_BUFFER_BIT);
    
    blurShaderH->use();
    blurShaderH->setInt("bloomTexture", 0);
    blurShaderH->setFloat("texelSizeX", 1.0f / (width / 4));
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, brightPassTexture);
    
    glBindVertexArray(screenQuadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    // Blur vertically
    glBindFramebuffer(GL_FRAMEBUFFER, blurFBO[1]);
    glClear(GL_COLOR_BUFFER_BIT);
    
    blurShaderV->use();
    blurShaderV->setInt("bloomTexture", 0);
    blurShaderV->setFloat("texelSizeY", 1.0f / (height / 4));
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, blurTexture[0]);
    
    glBindVertexArray(screenQuadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    glBindVertexArray(0);
    
    // Return the vertical blur result
    return blurTexture[1];
}

void Bloom::cleanup() {
    if (brightPassFBO) glDeleteFramebuffers(1, &brightPassFBO);
    if (brightPassTexture) glDeleteTextures(1, &brightPassTexture);
    
    glDeleteFramebuffers(2, blurFBO);
    glDeleteTextures(2, blurTexture);
    
    if (screenQuadVAO) glDeleteVertexArrays(1, &screenQuadVAO);
    if (screenQuadVBO) glDeleteBuffers(1, &screenQuadVBO);
}

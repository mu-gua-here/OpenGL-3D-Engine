#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>

class Shader;

class Bloom {
public:
    Bloom(int width, int height);
    ~Bloom();
    
    // Initialize bloom with screen dimensions
    void init(int width, int height);
    
    // Render bloom effect
    // Input: color texture to apply bloom to
    // Returns: bloom texture ID
    GLuint renderBloom(GLuint colorTexture);
    
    // Get the bloom result texture (at half resolution)
    GLuint getBloomTexture() const { return blurTexture[1]; }
    
    // Set bloom threshold (0.0 to 1.0, default 1.0)
    void setThreshold(float t) { bloomThreshold = t; }
    
    // Set bloom intensity (default 1.0)
    void setIntensity(float i) { bloomIntensity = i; }
    
    // Set number of blur passes (default 5)
    void setBlurPasses(int passes) { blurPasses = passes; }
    
    void cleanup();
    
    // Public access to intensity for compositing
    float bloomIntensity = 1.0f;

private:
    // Framebuffers and textures
    GLuint brightPassFBO = 0;
    GLuint brightPassTexture = 0;
    
    GLuint blurFBO[2] = {0, 0};
    GLuint blurTexture[2] = {0, 0};
    
    // Shaders
    std::unique_ptr<Shader> brightPassShader;
    std::unique_ptr<Shader> blurShaderH;
    std::unique_ptr<Shader> blurShaderV;
    
    // Screen quad VAO
    GLuint screenQuadVAO = 0;
    GLuint screenQuadVBO = 0;
    
    // Settings
    float bloomThreshold = 2.0f;
    int blurPasses = 5;
    
    // Dimensions
    int width = 0;
    int height = 0;
    
    void createFramebuffers();
    void createScreenQuad();
};

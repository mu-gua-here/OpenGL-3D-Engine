#include "shadowmap.h"

unsigned int SHADOW_WIDTH = 4096;
unsigned int SHADOW_HEIGHT = 4096;
GLuint shadowMapFBO = 0;
GLuint shadowMapTexture = 0;

void initShadowMap() {
    // Create framebuffer
    glGenFramebuffers(1, &shadowMapFBO);
    
    // Create depth texture
    glGenTextures(1, &shadowMapTexture);
    glBindTexture(GL_TEXTURE_2D, shadowMapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

    // GL_LINEAR for better filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    // Proper shadow comparison
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    
    // Attach depth texture to framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMapTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        printf("Error: Shadow map framebuffer is not complete\n");
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    printf("Shadowmap initialized (%dx%d)\n", SHADOW_WIDTH, SHADOW_HEIGHT);
}

void cleanupShadowMap() {
    if (shadowMapFBO != 0) glDeleteFramebuffers(1, &shadowMapFBO);
    if (shadowMapTexture != 0) glDeleteTextures(1, &shadowMapTexture);
}

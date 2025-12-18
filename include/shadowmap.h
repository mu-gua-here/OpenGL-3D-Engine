#pragma once
#include <glad/glad.h>
#include <stdio.h>

extern unsigned int SHADOW_WIDTH;
extern unsigned int SHADOW_HEIGHT;
extern GLuint shadowMapFBO;
extern GLuint shadowMapTexture;

// Shadow map initialization and cleanup
void initShadowMap();
void cleanupShadowMap();
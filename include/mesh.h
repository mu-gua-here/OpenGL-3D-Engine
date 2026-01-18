#pragma once

#include <glad/glad.h>
#include "material.h"
#include <vector>
#include <cstddef>

typedef struct {
    float u, v;
} Vec2;

typedef enum {
    CULL_NONE = 0, CULL_BACK = 1, CULL_FRONT = 2
} CullMode;

class Mesh {
public:
    std::vector<float> vertices_data;
    std::vector<unsigned int> indices_data;
    
    unsigned int TRIANGLE_COUNT;
    unsigned int INDEX_COUNT;
    GLuint VAO, VBO, EBO, instanceVBO;
    Material material;
    int cull_mode;
    bool is_cleaned_up;
    
    // NEW: Instance buffer properties
    size_t maxInstances = 1000;
    
    Mesh() : TRIANGLE_COUNT(0), VAO(0), VBO(0), EBO(0), instanceVBO(0), 
             cull_mode(CULL_NONE), is_cleaned_up(false) {
        material = createDefaultMaterial();
    }
    
    ~Mesh() {
        cleanup();
    }
    
    GLuint getVAO() const { return VAO; }
    bool isValid() const { return VAO != 0 && TRIANGLE_COUNT > 0 && !is_cleaned_up; }
    
    void cleanup() {
        if (is_cleaned_up) return;
        
        vertices_data.clear();
        indices_data.clear();
        
        if (VAO != 0) { glDeleteVertexArrays(1, &VAO); VAO = 0; }
        if (VBO != 0) { glDeleteBuffers(1, &VBO); VBO = 0; }
        if (EBO != 0) { glDeleteBuffers(1, &EBO); EBO = 0; }
        if (instanceVBO != 0) { glDeleteBuffers(1, &instanceVBO); instanceVBO = 0; }

        TRIANGLE_COUNT = INDEX_COUNT = 0;
        is_cleaned_up = true;
    }
};

//
//  obj_loader.h
//  OpenGL 3D Engine
//
//  Created by Ray Hsiao Muguang on 2025/6/24.
//

#ifndef OBJ_LOADER_H
#define OBJ_LOADER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    float x, y, z;
} Vec3;

typedef struct {
    float u, v;
} Vec2;

typedef struct {
    unsigned int v, vt, vn;  // vertex, texture, normal indices
} FaceVertex;

typedef struct {
    Vec3* vertices;
    Vec2* texCoords;
    Vec3* normals;
    FaceVertex* faces;
    
    int vertexCount;
    int texCoordCount;
    int normalCount;
    int faceCount;
} OBJModel;

// Function to parse OBJ file
OBJModel* loadOBJ(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Cannot open file %s\n", filename);
        return NULL;
    }
    
    OBJModel* model = malloc(sizeof(OBJModel));
    model->vertices = NULL;
    model->texCoords = NULL;
    model->normals = NULL;
    model->faces = NULL;
    model->vertexCount = 0;
    model->texCoordCount = 0;
    model->normalCount = 0;
    model->faceCount = 0;
    
    // First pass: count elements
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "v ", 2) == 0) model->vertexCount++;
        else if (strncmp(line, "vt ", 3) == 0) model->texCoordCount++;
        else if (strncmp(line, "vn ", 3) == 0) model->normalCount++;
        else if (strncmp(line, "f ", 2) == 0) model->faceCount += 2; // Assuming triangulation
    }
    
    // Allocate memory
    model->vertices = malloc(model->vertexCount * sizeof(Vec3));
    model->texCoords = malloc(model->texCoordCount * sizeof(Vec2));
    model->normals = malloc(model->normalCount * sizeof(Vec3));
    model->faces = malloc(model->faceCount * 3 * sizeof(FaceVertex)); // 3 vertices per triangle
    
    // Second pass: read data
    rewind(file);
    int vIndex = 0, vtIndex = 0, vnIndex = 0, fIndex = 0;
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "v ", 2) == 0) {
            sscanf(line, "v %f %f %f",
                   &model->vertices[vIndex].x,
                   &model->vertices[vIndex].y,
                   &model->vertices[vIndex].z);
            vIndex++;
        }
        else if (strncmp(line, "vt ", 3) == 0) {
            sscanf(line, "vt %f %f",
                   &model->texCoords[vtIndex].u,
                   &model->texCoords[vtIndex].v);
            vtIndex++;
        }
        else if (strncmp(line, "vn ", 3) == 0) {
            sscanf(line, "vn %f %f %f",
                   &model->normals[vnIndex].x,
                   &model->normals[vnIndex].y,
                   &model->normals[vnIndex].z);
            vnIndex++;
        }
        else if (strncmp(line, "f ", 2) == 0) {
            // Parse face (assuming triangles or quads)
            unsigned int v1, vt1, vn1, v2, vt2, vn2, v3, vt3, vn3, v4, vt4, vn4;
            int matches = sscanf(line, "f %u/%u/%u %u/%u/%u %u/%u/%u %u/%u/%u",
                                &v1, &vt1, &vn1, &v2, &vt2, &vn2,
                                &v3, &vt3, &vn3, &v4, &vt4, &vn4);
            
            if (matches == 12) { // Quad - split into two triangles
                // First triangle
                model->faces[fIndex].v = v1 - 1; model->faces[fIndex].vt = vt1 - 1; model->faces[fIndex].vn = vn1 - 1; fIndex++;
                model->faces[fIndex].v = v2 - 1; model->faces[fIndex].vt = vt2 - 1; model->faces[fIndex].vn = vn2 - 1; fIndex++;
                model->faces[fIndex].v = v3 - 1; model->faces[fIndex].vt = vt3 - 1; model->faces[fIndex].vn = vn3 - 1; fIndex++;
                
                // Second triangle
                model->faces[fIndex].v = v1 - 1; model->faces[fIndex].vt = vt1 - 1; model->faces[fIndex].vn = vn1 - 1; fIndex++;
                model->faces[fIndex].v = v3 - 1; model->faces[fIndex].vt = vt3 - 1; model->faces[fIndex].vn = vn3 - 1; fIndex++;
                model->faces[fIndex].v = v4 - 1; model->faces[fIndex].vt = vt4 - 1; model->faces[fIndex].vn = vn4 - 1; fIndex++;
            }
            else if (matches == 9) { // Triangle
                model->faces[fIndex].v = v1 - 1; model->faces[fIndex].vt = vt1 - 1; model->faces[fIndex].vn = vn1 - 1; fIndex++;
                model->faces[fIndex].v = v2 - 1; model->faces[fIndex].vt = vt2 - 1; model->faces[fIndex].vn = vn2 - 1; fIndex++;
                model->faces[fIndex].v = v3 - 1; model->faces[fIndex].vt = vt3 - 1; model->faces[fIndex].vn = vn3 - 1; fIndex++;
            }
        }
    }
    
    model->faceCount = fIndex;
    fclose(file);
    return model;
}

// Function to convert OBJ data to OpenGL vertex array
float* objToVertexArray(OBJModel* model, int* vertexCount) {
    *vertexCount = model->faceCount;
    
    // Each vertex: 3 pos + 3 normal + 2 texcoord = 8 floats
    float* vertices = malloc(model->faceCount * 8 * sizeof(float));
    
    for (int i = 0; i < model->faceCount; i++) {
        FaceVertex* face = &model->faces[i];
        int baseIndex = i * 8;
        
        // Position
        vertices[baseIndex + 0] = model->vertices[face->v].x;
        vertices[baseIndex + 1] = model->vertices[face->v].y;
        vertices[baseIndex + 2] = model->vertices[face->v].z;
        
        // Normal
        vertices[baseIndex + 3] = model->normals[face->vn].x;
        vertices[baseIndex + 4] = model->normals[face->vn].y;
        vertices[baseIndex + 5] = model->normals[face->vn].z;
        
        // Texture coordinates
        vertices[baseIndex + 6] = model->texCoords[face->vt].u;
        vertices[baseIndex + 7] = model->texCoords[face->vt].v;
    }
    
    return vertices;
}

// Function to free OBJ model memory
void freeOBJ(OBJModel* model) {
    if (model) {
        free(model->vertices);
        free(model->texCoords);
        free(model->normals);
        free(model->faces);
        free(model);
    }
}

// Usage example
void loadCubeModel() {
    OBJModel* cubeModel = loadOBJ("cube.obj");
    if (!cubeModel) return;
    
    int vertexCount;
    float* vertices = objToVertexArray(cubeModel, &vertexCount);
    
    // Now you can use 'vertices' with OpenGL
    // The vertex format is: [x, y, z, nx, ny, nz, u, v] per vertex
    
    // Set up your VAO/VBO here with the vertices array
    // glBufferData(GL_ARRAY_BUFFER, vertexCount * 8 * sizeof(float), vertices, GL_STATIC_DRAW);
    
    // Clean up
    free(vertices);
    freeOBJ(cubeModel);
}

#endif

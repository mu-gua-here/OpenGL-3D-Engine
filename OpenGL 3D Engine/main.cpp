//
//  main.mm
//  OpenGL Test
//
//  Created by Ray Hsiao Muguang on 2025/6/20.
//

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>
#include <OpenGL/gl3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <iostream>
#include <vector>
#include <memory>
#include <unordered_set>
#include "stb_image.h"

// --- Function Prototypes ---
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// Runtime
double fps;
bool paused = false;

// Engine variables
unsigned int total_triangles = 0;
unsigned int entity_count = 0;

// Global mesh registry for cleanup
std::unordered_set<class Mesh*> global_mesh_registry;

// FPS counter
double lastTime = 0.0;
unsigned int frameCount = 0;
double lastFrameTime = 0.0;

void updateFPS(GLFWwindow* window) {
    double currentTime = glfwGetTime();
    frameCount++;
    
    // Calculate frame time in milliseconds
    double frameTime = (currentTime - lastFrameTime) * 1000.0;
    lastFrameTime = currentTime;
    
    // Update FPS counter every second
    if (currentTime - lastTime >= 1.0) {
        fps = frameCount / (currentTime - lastTime);
        
        char title[256];
        snprintf(title, sizeof(title), "C++ OpenGL 3D Engine - FPS: %.1f | Frame Time: %.2f ms", fps, frameTime);
        glfwSetWindowTitle(window, title);
        frameCount = 0;
        lastTime = currentTime;
    }
}

// ============================================================================
// LOAD TEXTURES
// ============================================================================

GLuint loadTexture(const char* path) {
    int width, height, channels;
    
    // Load image data
    unsigned char* data = stbi_load(path, &width, &height, &channels, 0);
    if (!data) {
        std::cerr << "Failed to load texture: " << path << std::endl;
        return 0;
    }
    
    // Generate OpenGL texture
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    // Upload texture data
    GLenum format = 0; // Initialize format
    if (channels == 1) format = GL_RED;
    else if (channels == 3) format = GL_RGB;
    else if (channels == 4) format = GL_RGBA;
    else {
        std::cerr << "Unsupported texture format (channels: " << channels << ") for " << path << std::endl;
        stbi_image_free(data);
        return 0;
    }
    
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    // Clean up
    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return textureID;
}

// ============================================================================
// MATH LIBRARY
// ============================================================================

typedef struct {
    float x, y, z;
} Vec3;

typedef struct {
    float m[16]; // 4x4 matrix stored column-major
} Mat4;

// Basic vector operations
Vec3 vec3_add(Vec3 a, Vec3 b) {
    return (Vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 vec3_scale(Vec3 v, float s) {
    return (Vec3){v.x * s, v.y * s, v.z * s};
}

Vec3 vec3_sub(Vec3 a, Vec3 b) {
    return (Vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

float vec3_length(Vec3 v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

Vec3 vec3_normalize(Vec3 v) {
    float len = vec3_length(v);
    if (len == 0.0f) return (Vec3){0,0,0};
    return vec3_scale(v, 1.0f / len);
}

Vec3 vec3_cross(Vec3 a, Vec3 b) {
    return (Vec3){a.y * b.z - a.z * b.y,
                  a.z * b.x - a.x * b.z,
                  a.x * b.y - a.y * b.x};
}

float vec3_dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Matrix multiplication
Mat4 mat4_multiply(Mat4 a, Mat4 b) {
    Mat4 result = {0};
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            for (int k = 0; k < 4; k++) {
                result.m[i + j * 4] += a.m[i + k * 4] * b.m[k + j * 4];
            }
        }
    }
    return result;
}

// Identity matrix
Mat4 mat4_identity() {
    Mat4 m = {0};
    m.m[0] = m.m[5] = m.m[10] = m.m[15] = 1.0f;
    return m;
}

// Perspective projection matrix
Mat4 mat4_perspective(float fov, float aspect, float near, float far) {
    Mat4 m = {0};
    float f = 1.0f / tanf(fov * 0.5f);
    m.m[0] = f / aspect;
    m.m[5] = f;
    m.m[10] = (far + near) / (near - far);
    m.m[11] = -1.0f;
    m.m[14] = (2.0f * far * near) / (near - far);
    return m;
}

// Translation matrix
Mat4 mat4_translate(Vec3 pos) {
    Mat4 m = mat4_identity();
    m.m[12] = pos.x;
    m.m[13] = pos.y;
    m.m[14] = pos.z;
    return m;
}

// Rotation matrix around X axis (pitch)
Mat4 mat4_rotate_x(float angle) {
    Mat4 m = mat4_identity();
    float c = cosf(angle);
    float s = sinf(angle);
    m.m[5] = c;    m.m[6] = -s;
    m.m[9] = s;    m.m[10] = c;
    return m;
}

// Rotation matrix around Y axis (yaw)
Mat4 mat4_rotate_y(float angle) {
    Mat4 m = mat4_identity();
    float c = cosf(angle);
    float s = sinf(angle);
    m.m[0] = c;    m.m[2] = s;  // Corrected sign
    m.m[8] = -s;   m.m[10] = c; // Corrected sign
    return m;
}

// Rotation matrix around Z axis (roll)
Mat4 mat4_rotate_z(float angle) {
    Mat4 m = mat4_identity();
    float c = cosf(angle);
    float s = sinf(angle);
    m.m[0] = c;    m.m[1] = -s;
    m.m[4] = s;    m.m[5] = c;
    return m;
}

// Create scaling matrix
Mat4 mat4_scale(Vec3 scale) {
    Mat4 m = mat4_identity();
    m.m[0] = scale.x;
    m.m[5] = scale.y;
    m.m[10] = scale.z;
    return m;
}

// Lookat matrix
Mat4 mat4_look_at(Vec3 eye, Vec3 center, Vec3 up) {
    Vec3 f = vec3_normalize(vec3_sub(center, eye)); // Forward vector
    Vec3 s = vec3_normalize(vec3_cross(f, up));      // Side vector
    Vec3 u = vec3_cross(s, f);                       // Up vector (re-orthogonalized)

    Mat4 result = mat4_identity();
    result.m[0] = s.x;
    result.m[4] = s.y;
    result.m[8] = s.z;
    result.m[1] = u.x;
    result.m[5] = u.y;
    result.m[9] = u.z;
    result.m[2] = -f.x; // Negative Z because OpenGL is right-handed
    result.m[6] = -f.y;
    result.m[10] = -f.z;
    result.m[12] = -vec3_dot(s, eye);
    result.m[13] = -vec3_dot(u, eye);
    result.m[14] = vec3_dot(f, eye);
    return result;
}

// Engine
Mat4 view;
Mat4 projection;

// ============================================================================
// MESH SYSTEM (3D Object representation)
// ============================================================================

// UV coordinates
typedef struct {
    float u, v;
} Vec2;

// RGBA color structure
typedef struct {
    float r, g, b, a;
} Color;

// Backface culling modes
typedef enum {
    CULL_NONE = 0,
    CULL_BACK = 1,
    CULL_FRONT = 2
} CullMode;

// ============================================================================
// MESH TEMPLATE SYSTEM
// ============================================================================

class Mesh {
public:
    float* vertices;
    unsigned int* indices;
    size_t vertex_count;
    size_t index_count;
    unsigned int TRIANGLE_COUNT;
    GLuint VAO, VBO, EBO;
    GLuint texture_id;
    CullMode cull_mode;
    bool is_cleaned_up;
    
    // Constructor
    Mesh() : vertices(nullptr), indices(nullptr), vertex_count(0), index_count(0),
             TRIANGLE_COUNT(0), VAO(0), VBO(0), EBO(0), texture_id(0),
             cull_mode(CULL_BACK), is_cleaned_up(false) {
        // Use unordered_set instead of vector to avoid reallocation issues
        global_mesh_registry.insert(this);
    }
    
    // Destructor
    ~Mesh() {
        cleanup();
        global_mesh_registry.erase(this);
    }
    
    // Get VAO for debugging
    GLuint getVAO() const { return VAO; }
    
    // Check if mesh is valid
    bool isValid() const {
        return VAO != 0 && TRIANGLE_COUNT > 0 && !is_cleaned_up;
    }
    
    // Cleanup method - can be called safely multiple times
    void cleanup() {
        if (is_cleaned_up) return;
        
        // Free CPU memory
        if (vertices) {
            free(vertices);
            vertices = nullptr;
        }
        if (indices) {
            free(indices);
            indices = nullptr;
        }
        
        // Delete OpenGL objects
        if (VAO != 0) {
            glDeleteVertexArrays(1, &VAO);
            VAO = 0;
        }
        if (VBO != 0) {
            glDeleteBuffers(1, &VBO);
            VBO = 0;
        }
        if (EBO != 0) {
            glDeleteBuffers(1, &EBO);
            EBO = 0;
        }
        
        vertex_count = index_count = TRIANGLE_COUNT = 0;
        is_cleaned_up = true;
    }
};

void cleanup_all_meshes() {
    for (auto* mesh : global_mesh_registry) {
        if (mesh && !mesh->is_cleaned_up) {
            mesh->cleanup();
        }
    }
    global_mesh_registry.clear();
}

// ============================================================================
// SKYBOX CUBE
// ============================================================================

GLuint loadCubemap(const char* faces[6]) {
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, channels;
    for (unsigned int i = 0; i < 6; i++) {
        unsigned char* data = stbi_load(faces[i], &width, &height, &channels, 0);
        if (data) {
            GLenum format = GL_RGB;
            if (channels == 1) format = GL_RED;
            else if (channels == 3) format = GL_RGB;
            else if (channels == 4) format = GL_RGBA;

            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                        0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        } else {
            std::cerr << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    
    return textureID;
}

static const float SKYBOX_VERTICES[] = {
    // positions
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f
};

// ============================================================================
// ENTITY STRUCTURES
// ============================================================================

typedef struct {
    const char* name;
    Vec3 position;
    Vec3 rotation;
    Vec3 scale;
    Mesh* mesh;
    int active;
} Entity;

// Create entity
Entity create_entity(const char* name, Mesh* mesh, Vec3 pos) {
    Entity entity;
    entity.name = name;
    entity.position = pos;
    entity.rotation = (Vec3){0, 0, 0};
    entity.scale = (Vec3){1, 1, 1};
    entity.mesh = mesh;
    entity.active = 1;
    return entity;
}

// Global dynamic array
std::vector<Entity> all_entities;

void update_entity(const char* name, Vec3 pos, Vec3 rot, Vec3 scale) {
    // Search for entity with given name
    for (unsigned int i = 0; i < all_entities.size(); i++) {
        if (all_entities[i].name == name) {
            // Now update entity attributes
            all_entities[i].position.x = pos.x;
            all_entities[i].position.y = pos.y;
            all_entities[i].position.z = pos.z;
            
            all_entities[i].rotation.x = rot.x;
            all_entities[i].rotation.y = rot.y;
            all_entities[i].rotation.z = rot.z;
            
            all_entities[i].scale.x = scale.x;
            all_entities[i].scale.y = scale.y;
            all_entities[i].scale.z = scale.z;
        }
    }
}

// ============================================================================
// OBJ AND MTL FILE IMPORTERS
// ============================================================================

typedef struct {
    unsigned int v, vt, vn;  // vertex, texture, normal indices
} FaceVertex;

typedef struct {
    char name[256];
    GLuint texture_id;
    Color diffuse_color;
} Material;

typedef struct {
    Vec3* vertices;
    Vec2* texCoords;
    Vec3* normals;
    FaceVertex* faces;
    Material* materials;
    int* face_materials;
    
    size_t vertexCount;
    size_t texCoordCount;
    size_t normalCount;
    size_t faceCount;
    size_t materialCount;
} OBJModel;

OBJModel* loadOBJ(const char* filename, const char* mtl_path = nullptr) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Cannot open file %s\n", filename);
        return nullptr;
    }
    
    // Use safer allocation approach
    OBJModel* model = (OBJModel*)calloc(1, sizeof(OBJModel));
    if (!model) {
        fclose(file);
        return nullptr;
    }
    
    // Declare ALL variables at the top to avoid goto issues
    char line[1024];
    std::vector<std::string> materials;
    size_t vIndex = 0, vtIndex = 0, vnIndex = 0, fIndex = 0;
    int current_material = 0;
    size_t face_count = 0;
    bool allocation_failed = false;
    
    // First pass: count elements
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "v ", 2) == 0) {
            model->vertexCount++;
        }
        else if (strncmp(line, "vt ", 3) == 0) {
            model->texCoordCount++;
        }
        else if (strncmp(line, "vn ", 3) == 0) {
            model->normalCount++;
        }
        else if (strncmp(line, "f ", 2) == 0) {
            // Count vertices in face (handle both triangles and quads)
            int vertex_count = 0;
            char* line_copy = strdup(line); // Make a copy for strtok
            if (line_copy) {
                char* token = strtok(line_copy + 2, " \t\n");
                while (token) {
                    vertex_count++;
                    token = strtok(NULL, " \t\n");
                }
                free(line_copy);
                
                if (vertex_count == 4) {
                    model->faceCount += 2; // Quad -> 2 triangles
                } else if (vertex_count == 3) {
                    model->faceCount += 1; // Triangle
                }
            }
        }
        else if (strncmp(line, "usemtl ", 7) == 0) {
            char mtl_name[256];
            if (sscanf(line, "usemtl %255s", mtl_name) == 1) {
                bool found = false;
                for (const auto& existing : materials) {
                    if (existing == mtl_name) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    materials.push_back(mtl_name);
                }
            }
        }
    }
    
    model->materialCount = std::max((size_t)1, materials.size());
    
    // Allocate arrays with safety checks - use if/else chain instead of goto
    if (model->vertexCount > 0) {
        model->vertices = (Vec3*)calloc(model->vertexCount, sizeof(Vec3));
        if (!model->vertices) allocation_failed = true;
    }
    
    if (!allocation_failed && model->texCoordCount > 0) {
        model->texCoords = (Vec2*)calloc(model->texCoordCount, sizeof(Vec2));
        if (!model->texCoords) allocation_failed = true;
    }
    
    if (!allocation_failed && model->normalCount > 0) {
        model->normals = (Vec3*)calloc(model->normalCount, sizeof(Vec3));
        if (!model->normals) allocation_failed = true;
    }
    
    if (!allocation_failed && model->faceCount > 0) {
        model->faces = (FaceVertex*)calloc(model->faceCount * 3, sizeof(FaceVertex));
        if (!model->faces) allocation_failed = true;
        
        if (!allocation_failed) {
            model->face_materials = (int*)calloc(model->faceCount, sizeof(int));
            if (!model->face_materials) allocation_failed = true;
        }
    }
    
    if (!allocation_failed) {
        model->materials = (Material*)calloc(model->materialCount, sizeof(Material));
        if (!model->materials) allocation_failed = true;
    }
    
    // If any allocation failed, clean up and return null
    if (allocation_failed) {
        printf("Error: Failed to allocate memory for OBJ model\n");
        if (model->vertices) free(model->vertices);
        if (model->texCoords) free(model->texCoords);
        if (model->normals) free(model->normals);
        if (model->faces) free(model->faces);
        if (model->materials) free(model->materials);
        if (model->face_materials) free(model->face_materials);
        free(model);
        fclose(file);
        return nullptr;
    }
    
    // Initialize materials
    for (size_t i = 0; i < materials.size(); i++) {
        strncpy(model->materials[i].name, materials[i].c_str(), 255);
        model->materials[i].name[255] = '\0';
        model->materials[i].texture_id = 0;
        model->materials[i].diffuse_color = (Color){1.0f, 1.0f, 1.0f, 1.0f};
    }
    
    // If no materials found, create default
    if (materials.empty()) {
        strcpy(model->materials[0].name, "Default");
        model->materials[0].texture_id = 0;
        model->materials[0].diffuse_color = (Color){1.0f, 1.0f, 1.0f, 1.0f};
    }
    
    // Second pass: read data with bounds checking
    rewind(file);
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "v ", 2) == 0 && vIndex < model->vertexCount) {
            sscanf(line, "v %f %f %f",
                   &model->vertices[vIndex].x,
                   &model->vertices[vIndex].y,
                   &model->vertices[vIndex].z);
            vIndex++;
        }
        else if (strncmp(line, "vt ", 3) == 0 && vtIndex < model->texCoordCount) {
            sscanf(line, "vt %f %f",
                   &model->texCoords[vtIndex].u,
                   &model->texCoords[vtIndex].v);
            vtIndex++;
        }
        else if (strncmp(line, "vn ", 3) == 0 && vnIndex < model->normalCount) {
            sscanf(line, "vn %f %f %f",
                   &model->normals[vnIndex].x,
                   &model->normals[vnIndex].y,
                   &model->normals[vnIndex].z);
            vnIndex++;
        }
        else if (strncmp(line, "usemtl ", 7) == 0) {
            char mtl_name[256];
            if (sscanf(line, "usemtl %255s", mtl_name) == 1) {
                bool found = false;
                for (int i = 0; i < (int)model->materialCount; i++) {
                    if (strcmp(model->materials[i].name, mtl_name) == 0) {
                        current_material = i;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    current_material = 0;
                }
            }
        }
        else if (strncmp(line, "f ", 2) == 0 && face_count < model->faceCount) {
            unsigned int v1, vt1, vn1, v2, vt2, vn2, v3, vt3, vn3, v4, vt4, vn4;
            
            // Try to parse as quad first
            int matches = sscanf(line, "f %u/%u/%u %u/%u/%u %u/%u/%u %u/%u/%u",
                                &v1, &vt1, &vn1, &v2, &vt2, &vn2,
                                &v3, &vt3, &vn3, &v4, &vt4, &vn4);
            
            if (matches == 12 && face_count + 1 < model->faceCount) {
                // Validate all indices before processing
                bool quad_valid = (v1 > 0 && v1 <= model->vertexCount && vt1 > 0 && vt1 <= model->texCoordCount && vn1 > 0 && vn1 <= model->normalCount &&
                                  v2 > 0 && v2 <= model->vertexCount && vt2 > 0 && vt2 <= model->texCoordCount && vn2 > 0 && vn2 <= model->normalCount &&
                                  v3 > 0 && v3 <= model->vertexCount && vt3 > 0 && vt3 <= model->texCoordCount && vn3 > 0 && vn3 <= model->normalCount &&
                                  v4 > 0 && v4 <= model->vertexCount && vt4 > 0 && vt4 <= model->texCoordCount && vn4 > 0 && vn4 <= model->normalCount);
                
                if (quad_valid) {
                    // First triangle: v1, v2, v3
                    model->faces[fIndex].v = v1 - 1; model->faces[fIndex].vt = vt1 - 1; model->faces[fIndex].vn = vn1 - 1; fIndex++;
                    model->faces[fIndex].v = v2 - 1; model->faces[fIndex].vt = vt2 - 1; model->faces[fIndex].vn = vn2 - 1; fIndex++;
                    model->faces[fIndex].v = v3 - 1; model->faces[fIndex].vt = vt3 - 1; model->faces[fIndex].vn = vn3 - 1; fIndex++;
                    model->face_materials[face_count] = current_material;
                    face_count++;
                    
                    // Second triangle: v1, v3, v4
                    model->faces[fIndex].v = v1 - 1; model->faces[fIndex].vt = vt1 - 1; model->faces[fIndex].vn = vn1 - 1; fIndex++;
                    model->faces[fIndex].v = v3 - 1; model->faces[fIndex].vt = vt3 - 1; model->faces[fIndex].vn = vn3 - 1; fIndex++;
                    model->faces[fIndex].v = v4 - 1; model->faces[fIndex].vt = vt4 - 1; model->faces[fIndex].vn = vn4 - 1; fIndex++;
                    model->face_materials[face_count] = current_material;
                    face_count++;
                }
            }
            else {
                // Try triangle
                matches = sscanf(line, "f %u/%u/%u %u/%u/%u %u/%u/%u",
                                &v1, &vt1, &vn1, &v2, &vt2, &vn2, &v3, &vt3, &vn3);
                
                if (matches == 9) {
                    bool triangle_valid = (v1 > 0 && v1 <= model->vertexCount && vt1 > 0 && vt1 <= model->texCoordCount && vn1 > 0 && vn1 <= model->normalCount &&
                                          v2 > 0 && v2 <= model->vertexCount && vt2 > 0 && vt2 <= model->texCoordCount && vn2 > 0 && vn2 <= model->normalCount &&
                                          v3 > 0 && v3 <= model->vertexCount && vt3 > 0 && vt3 <= model->texCoordCount && vn3 > 0 && vn3 <= model->normalCount);
                    
                    if (triangle_valid) {
                        model->faces[fIndex].v = v1 - 1; model->faces[fIndex].vt = vt1 - 1; model->faces[fIndex].vn = vn1 - 1; fIndex++;
                        model->faces[fIndex].v = v2 - 1; model->faces[fIndex].vt = vt2 - 1; model->faces[fIndex].vn = vn2 - 1; fIndex++;
                        model->faces[fIndex].v = v3 - 1; model->faces[fIndex].vt = vt3 - 1; model->faces[fIndex].vn = vn3 - 1; fIndex++;
                        model->face_materials[face_count] = current_material;
                        face_count++;
                    }
                }
            }
        }
    }
    
    fclose(file);
    return model;
}

// Function to parse MTL file
void loadMTL(const char* mtl_path, Material* materials, size_t* material_count, size_t max_materials) {
    FILE* file = fopen(mtl_path, "r");
    if (!file) {
        printf("Warning: Cannot open MTL file %s\n", mtl_path);
        return;
    }
    
    char line[256];
    int current_material = -1;
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "newmtl ", 7) == 0) {
            if (current_material + 1 < max_materials) {
                current_material++;
                char mtl_name[256];
                sscanf(line, "newmtl %s", mtl_name);
                strcpy(materials[current_material].name, mtl_name);
            }
        }
        // Diffuse color
        else if (strncmp(line, "Kd ", 3) == 0 && current_material >= 0) {
            float r, g, b;
            sscanf(line, "Kd %f %f %f", &r, &g, &b);
            materials[current_material].diffuse_color = (Color){r, g, b, 1.0f};
        }
        // Diffuse texture map
        else if (strncmp(line, "map_Kd ", 7) == 0 && current_material >= 0) {
            char texture_path[256];
            sscanf(line, "map_Kd %s", texture_path);
            
            // Load the texture
            materials[current_material].texture_id = loadTexture(texture_path);
            if (materials[current_material].texture_id == 0) {
                printf("Warning: Failed to load texture %s for material %s\n",
                       texture_path, materials[current_material].name);
            }
        }
    }
    
    *material_count = current_material + 1;
    fclose(file);
}

OBJModel* loadOBJWithMTL(const char* obj_path) {
    // Load OBJ structure first
    OBJModel* model = loadOBJ(obj_path);
    if (!model) return nullptr;
    
    // Look for MTL file reference in OBJ
    FILE* obj_file = fopen(obj_path, "r");
    if (obj_file) {
        char line[256];
        char mtl_file[256] = {0};
        
        while (fgets(line, sizeof(line), obj_file)) {
            if (strncmp(line, "mtllib ", 7) == 0) {
                sscanf(line, "mtllib %s", mtl_file);
                break;
            }
        }
        fclose(obj_file);
        
        // Load MTL file if found
        if (strlen(mtl_file) > 0) {
            // Construct full path (assuming MTL is in same directory as OBJ)
            char mtl_path[512];
            const char* last_slash = strrchr(obj_path, '/');
            if (last_slash) {
                size_t dir_len = last_slash - obj_path + 1;
                strncpy(mtl_path, obj_path, dir_len);
                mtl_path[dir_len] = '\0';
                strcat(mtl_path, mtl_file);
            } else {
                strcpy(mtl_path, mtl_file);
            }
            
            loadMTL(mtl_path, model->materials, &model->materialCount, model->materialCount);
        }
    }
    
    return model;
}

std::vector<Mesh*> create_mesh_with_obj(const char* name, const char* obj_path, Vec3 center, float size) {
    OBJModel* model = loadOBJWithMTL(obj_path);
    if (!model) {
        printf("Failed to load OBJ file with materials: %s\n", obj_path);
        return {};
    }
    
    std::vector<Mesh*> meshes;
    
    // Create separate mesh for each material
    for (size_t mat_idx = 0; mat_idx < model->materialCount; mat_idx++) {
        std::vector<float> vertices;
        int triangle_count = 0;
        
        // Collect all faces that use this material
        for (size_t face_idx = 0; face_idx < model->faceCount; face_idx++) {
            if (model->face_materials[face_idx] != (int)mat_idx) {
                continue;
            }
            
            // Validate triangle indices
            bool triangle_valid = true;
            for (int v = 0; v < 3; v++) {
                size_t vertex_idx = face_idx * 3 + v;
                if (vertex_idx >= model->faceCount * 3) {
                    triangle_valid = false;
                    break;
                }
                
                FaceVertex* face = &model->faces[vertex_idx];
                
                if (face->v >= model->vertexCount ||
                    face->vt >= model->texCoordCount ||
                    face->vn >= model->normalCount) {
                    triangle_valid = false;
                    break;
                }
            }
            
            if (!triangle_valid) continue;
            
            // Process all 3 vertices of the valid triangle
            for (int v = 0; v < 3; v++) {
                size_t vertex_idx = face_idx * 3 + v;
                FaceVertex* face = &model->faces[vertex_idx];
                
                // Position
                Vec3 pos = model->vertices[face->v];
                vertices.push_back(pos.x * size + center.x);
                vertices.push_back(pos.y * size + center.y);
                vertices.push_back(pos.z * size + center.z);
                
                // Color
                vertices.push_back(model->materials[mat_idx].diffuse_color.r);
                vertices.push_back(model->materials[mat_idx].diffuse_color.g);
                vertices.push_back(model->materials[mat_idx].diffuse_color.b);
                vertices.push_back(model->materials[mat_idx].diffuse_color.a);
                
                // Texture coordinates
                vertices.push_back(model->texCoords[face->vt].u);
                vertices.push_back(model->texCoords[face->vt].v);
                
                // Normals
                vertices.push_back(model->normals[face->vn].x);
                vertices.push_back(model->normals[face->vn].y);
                vertices.push_back(model->normals[face->vn].z);
            }
            triangle_count++;
        }
        
        // Create mesh if it has triangles
        if (triangle_count > 0) {
            Mesh* mesh = new Mesh();
            mesh->vertex_count = vertices.size();
            mesh->index_count = triangle_count * 3;
            mesh->TRIANGLE_COUNT = triangle_count;
            mesh->texture_id = model->materials[mat_idx].texture_id;
            mesh->cull_mode = CULL_NONE;
            
            // Allocate vertex data
            mesh->vertices = (float*)malloc(mesh->vertex_count * sizeof(float));
            if (!mesh->vertices) {
                delete mesh;
                continue;
            }
            memcpy(mesh->vertices, vertices.data(), mesh->vertex_count * sizeof(float));
            mesh->indices = nullptr;
            
            // Create OpenGL objects
            glGenVertexArrays(1, &mesh->VAO);
            glGenBuffers(1, &mesh->VBO);
            
            if (mesh->VAO == 0 || mesh->VBO == 0) {
                printf("Failed to create OpenGL objects\n");
                delete mesh;
                continue;
            }
            
            glBindVertexArray(mesh->VAO);
            glBindBuffer(GL_ARRAY_BUFFER, mesh->VBO);
            glBufferData(GL_ARRAY_BUFFER, mesh->vertex_count * sizeof(float), mesh->vertices, GL_STATIC_DRAW);
            
            // Set up vertex attributes
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(7 * sizeof(float)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(9 * sizeof(float)));
            glEnableVertexAttribArray(3);
            
            glBindVertexArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            
            meshes.push_back(mesh);
            total_triangles += mesh->TRIANGLE_COUNT;
        }
    }
    
    // Clean up model
    if (model->vertices) free(model->vertices);
    if (model->texCoords) free(model->texCoords);
    if (model->normals) free(model->normals);
    if (model->faces) free(model->faces);
    if (model->materials) free(model->materials);
    if (model->face_materials) free(model->face_materials);
    free(model);
        
    // Create entities
    for (Mesh* mesh : meshes) {
        all_entities.push_back(create_entity(name, mesh, center));
    }
    
    entity_count++;
    return meshes;
}

// ============================================================================
// CAMERA SYSTEM
// ============================================================================

typedef struct {
    Vec3 position;
    Vec3 front; // Direction camera is looking
    Vec3 up;    // Up direction
    Vec3 right; // Right direction

    float yaw;   // Y-axis rotation
    float pitch; // X-axis rotation

    float fov;
    float aspect_ratio;
    float near_plane;
    float far_plane;
} Camera;

Camera global_camera;

Camera create_camera(float aspect) {
    Camera cam;
    cam.position = (Vec3){0, 0, 5};
    cam.front = (Vec3){0, 0, -1};
    cam.up = (Vec3){0, 1, 0};
    cam.right = (Vec3){1, 0, 0};

    cam.yaw = -90.0f;
    cam.pitch = 0.0f;

    cam.fov = M_PI / 4.0f; // 45 degrees in radians
    cam.aspect_ratio = aspect;
    cam.near_plane = 0.1f;
    cam.far_plane = 100.0f;

    return cam;
}

// Function to update camera's front, right, and up vectors based on yaw and pitch
void camera_update_vectors(Camera* cam) {
    // Calculate new front vector
    cam->front.x = cosf(cam->yaw * (M_PI / 180.0f)) * cosf(cam->pitch * (M_PI / 180.0f));
    cam->front.y = sinf(cam->pitch * (M_PI / 180.0f));
    cam->front.z = sinf(cam->yaw * (M_PI / 180.0f)) * cosf(cam->pitch * (M_PI / 180.0f));
    cam->front = vec3_normalize(cam->front);

    // Calculate right and up vector
    // OpenGL's world up is (0,1,0)
    cam->right = vec3_normalize(vec3_cross(cam->front, (Vec3){0, 1, 0}));
    cam->up = vec3_normalize(vec3_cross(cam->right, cam->front));
}

Mat4 camera_get_projection(Camera* cam) {
    return mat4_perspective(cam->fov, cam->aspect_ratio, cam->near_plane, cam->far_plane);
}

// Calculates the view matrix using the LookAt function
Mat4 camera_get_view_matrix(Camera* cam) {
    Vec3 target = vec3_add(cam->position, cam->front);
    return mat4_look_at(cam->position, target, cam->up);
}

// ============================================================================
// SHADER SYSTEM
// ============================================================================

typedef struct {
    unsigned int program;
    int mvp_location; // Model-View-Projection matrix uniform location
    int texture_location;
} Shader;

const char* vertex_shader = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec4 aColor;\n"
    "layout (location = 2) in vec2 aTexCoord;\n"
    "out vec4 vertexColor;\n"
    "out vec2 TexCoord;\n"
    "uniform mat4 uMVP;\n"
    "void main() {\n"
    "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
    "    vertexColor = aColor;\n"
    "    TexCoord = aTexCoord;\n"
    "}\0";

const char* fragment_shader = "#version 330 core\n"
    "in vec4 vertexColor;\n"
    "in vec2 TexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D u_texture;\n"
    "void main() {\n"
    "   FragColor = texture(u_texture, TexCoord) * vertexColor;\n"
    "   if (FragColor.a < 0.1f) {\n"
    "       discard;\n"
    "   }\n"
    "}\0";

Shader create_shader() {
    // Compile vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertex_shader, NULL);
    glCompileShader(vertexShader);
    
    // Check vertex shader compilation
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        printf("Vertex shader compilation failed: %s\n", infoLog);
    }

    // Compile fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragment_shader, NULL);
    glCompileShader(fragmentShader);
    
    // Check fragment shader compilation
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        printf("Fragment shader compilation failed: %s\n", infoLog);
    }

    // Link shader program
    unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    // Check program linking
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        printf("Shader program linking failed: %s\n", infoLog);
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    Shader shader;
    shader.program = program;
    shader.mvp_location = glGetUniformLocation(program, "uMVP");
    shader.texture_location = glGetUniformLocation(program, "u_texture");

    return shader;
}

// ============================================================================
// SKYBOX SHADER SYSTEM
// ============================================================================

typedef struct {
    unsigned int program;
    int view_location;
    int projection_location;
    int skybox_location;
} SkyboxShader;

const char* skybox_vertex_shader = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "out vec3 TexCoords;\n"
    "uniform mat4 projection;\n"
    "uniform mat4 view;\n"
    "void main() {\n"
    "    TexCoords = aPos;\n"
    "    vec4 pos = projection * view * vec4(aPos, 1.0);\n"
    "    gl_Position = pos.xyww;\n" // Make skybox always at far plane
    "}\0";

const char* skybox_fragment_shader = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "in vec3 TexCoords;\n"
    "uniform samplerCube skybox;\n"
    "void main() {\n"
    "    FragColor = texture(skybox, TexCoords);\n"
    "}\0";

SkyboxShader create_skybox_shader() {
    // Compile vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &skybox_vertex_shader, NULL);
    glCompileShader(vertexShader);
    
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        printf("Skybox vertex shader compilation failed: %s\n", infoLog);
    }

    // Compile fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &skybox_fragment_shader, NULL);
    glCompileShader(fragmentShader);
    
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        printf("Skybox fragment shader compilation failed: %s\n", infoLog);
    }

    // Link shader program
    unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        printf("Skybox shader program linking failed: %s\n", infoLog);
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    SkyboxShader shader;
    shader.program = program;
    shader.view_location = glGetUniformLocation(program, "view");
    shader.projection_location = glGetUniformLocation(program, "projection");
    shader.skybox_location = glGetUniformLocation(program, "skybox");

    return shader;
}

// ============================================================================
// RENDERER SYSTEM
// ============================================================================

typedef struct {
    Shader shader;
} Renderer;

void renderer_init(Renderer* renderer, float aspect) {
    renderer->shader = create_shader();
    global_camera = create_camera(aspect);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.1f);
}

void draw_obj_mesh(Renderer* renderer, Entity* entity, Mesh* mesh) {
    if (!entity->active || mesh->TRIANGLE_COUNT == 0) return;
    
    // Set up culling based on mesh's cull mode
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
    
    // Calculate transformation matrices
    Mat4 scale_matrix = mat4_scale(entity->scale);
    Mat4 rotation_x = mat4_rotate_x(entity->rotation.x);
    Mat4 rotation_y = mat4_rotate_y(entity->rotation.y);
    Mat4 rotation_z = mat4_rotate_z(entity->rotation.z);
    Mat4 rotation = mat4_multiply(rotation_z, mat4_multiply(rotation_y, rotation_x));
    Mat4 translation = mat4_translate(entity->position);
    Mat4 model = mat4_multiply(translation, mat4_multiply(rotation, scale_matrix));
    Mat4 mvp = mat4_multiply(projection, mat4_multiply(view, model));
    
    // Use shader and set uniforms
    glUseProgram(renderer->shader.program);
    glUniformMatrix4fv(renderer->shader.mvp_location, 1, GL_FALSE, mvp.m);
    
    // Bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mesh->texture_id);
    glUniform1i(renderer->shader.texture_location, 0);
    
    // Draw vertices
    glBindVertexArray(mesh->VAO);
    glDrawArrays(GL_TRIANGLES, 0, mesh->TRIANGLE_COUNT * 3);
    glBindVertexArray(0);
}

// ============================================================================
// SKYBOX CLASS
// ============================================================================

class Skybox {
public:
    GLuint VAO, VBO;
    GLuint cubemap_texture;
    SkyboxShader shader;
    
    Skybox() : VAO(0), VBO(0), cubemap_texture(0) {}
    
    ~Skybox() {
        cleanup();
    }
    
    void init(const char* faces[6]) {
        // Load cubemap texture
        cubemap_texture = loadCubemap(faces);
        
        // Create shader
        shader = create_skybox_shader();
        
        // Set up vertex data
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(SKYBOX_VERTICES), &SKYBOX_VERTICES, GL_STATIC_DRAW);
        
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        
        glBindVertexArray(0);
    }
    
    void render(Camera* camera) {
        // Change depth function so depth test passes when values are equal to depth buffer's content
        glDepthFunc(GL_LEQUAL);
        
        glUseProgram(shader.program);
        
        // Remove translation from the view matrix (only keep rotation)
        Mat4 view = camera_get_view_matrix(camera);
        // Zero out the translation part (last column)
        view.m[12] = 0.0f;
        view.m[13] = 0.0f;
        view.m[14] = 0.0f;
        
        Mat4 projection = camera_get_projection(camera);
        
        glUniformMatrix4fv(shader.view_location, 1, GL_FALSE, view.m);
        glUniformMatrix4fv(shader.projection_location, 1, GL_FALSE, projection.m);
        
        // Bind skybox texture
        glBindVertexArray(VAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap_texture);
        glUniform1i(shader.skybox_location, 0);
        
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        
        // Set depth function back to default
        glDepthFunc(GL_LESS);
    }
    
    void cleanup() {
        if (VAO != 0) glDeleteVertexArrays(1, &VAO);
        if (VBO != 0) glDeleteBuffers(1, &VBO);
        if (cubemap_texture != 0) glDeleteTextures(1, &cubemap_texture);
        if (shader.program != 0) glDeleteProgram(shader.program);
        
        VAO = VBO = cubemap_texture = 0;
        shader.program = 0;
    }
};

// ============================================================================
// MAIN ENGINE LOOP
// ============================================================================

int main() {
    // Initialize GLFW and create window
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    
    GLFWwindow* window = glfwCreateWindow(800, 600, "C++ OpenGL 3D Engine", NULL, NULL);
    if (!window) {
        printf("Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    
    // Set callbacks
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    
    // Initialize renderer
    Renderer renderer;
    renderer_init(&renderer, 800.0f / 600.0f);
    camera_update_vectors(&global_camera);
    
    // Check for shader errors immediately after creation
    if (renderer.shader.program == 0) {
        printf("Shader program creation failed. Exiting.\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }
    
    // ============================================================================
    // LOAD ALL TEXTURES
    // ============================================================================
    
    const char* skybox_faces[6] = {
        "cloud_skybox right.png",   // GL_TEXTURE_CUBE_MAP_POSITIVE_X
        "cloud_skybox left.png",    // GL_TEXTURE_CUBE_MAP_NEGATIVE_X
        "cloud_skybox top.png",     // GL_TEXTURE_CUBE_MAP_POSITIVE_Y
        "cloud_skybox bottom.png",  // GL_TEXTURE_CUBE_MAP_NEGATIVE_Y
        "cloud_skybox front.png",   // GL_TEXTURE_CUBE_MAP_POSITIVE_Z
        "cloud_skybox back.png"     // GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
    };
    Skybox skybox;
    skybox.init(skybox_faces);
    
    // ============================================================================
    // LOAD AND CREATE ALL GAME OBJECTS
    // ============================================================================
    
    // Create all scene entities
    create_mesh_with_obj("tree_mesh", "tree.obj", (Vec3){0, 0, 0}, 2.0f);
    
    printf("Total triangles: %d\n", total_triangles);
    printf("Game objects: %d\n", entity_count);
    printf("Controls: WASD to move, mouse to look around, E/Q to move up/down, ESC to exit\n");
    
    // ENGINE MAIN LOOP //
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents(); // This processes events and calls mouse_callback
        updateFPS(window);
        
        if (paused == false) {
            // Keyboard input for camera movement
            
            float yaw_rad = global_camera.yaw * M_PI / 180.0f;
            float sin_yaw = sinf(yaw_rad);
            float cos_yaw = cosf(yaw_rad);
            Vec3 cam_offset = {0, 0, 0};
            
            float camera_speed = 0.05f;
            if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
                camera_speed *= 2;
            }
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
                cam_offset = vec3_add(cam_offset, (Vec3){cos_yaw, 0, sin_yaw});
            }
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
                cam_offset = vec3_add(cam_offset, (Vec3){-cos_yaw, 0, -sin_yaw});
            }
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
                cam_offset = vec3_add(cam_offset, (Vec3){sin_yaw, 0, -cos_yaw});
            }
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                cam_offset = vec3_add(cam_offset, (Vec3){-sin_yaw, 0, cos_yaw});
            }
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
                global_camera.position.y += camera_speed;
            }
            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
                global_camera.position.y -= camera_speed;
            }
            cam_offset = vec3_normalize(cam_offset);
            global_camera.position = vec3_add(global_camera.position, vec3_scale(cam_offset, camera_speed));
            
            view = camera_get_view_matrix(&global_camera);
            projection = camera_get_projection(&global_camera);
            
            
            // ============================================================================
            // UPDATE ENTITIES
            // ============================================================================
            
            update_entity("tree_mesh", (Vec3){static_cast<float>(frameCount), 0, 0}, (Vec3){0, 0, 0}, (Vec3){1, 1, 1});
        }
        
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        
        // ============================================================================
        // RENDER ALL SCENE OBJECTS
        // ============================================================================
        
        // Render skybox first
        skybox.render(&global_camera);
        
        // Render regular objects
        for (unsigned int i = 0; i < all_entities.size(); i++) {
            draw_obj_mesh(&renderer, &all_entities[i], all_entities[i].mesh);
        }
        
        glfwSwapBuffers(window);
        
        // Set cursor modes
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
                paused = true;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); // Disable pointerlock
            }
        }
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) {
            if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_NORMAL) {
                paused = false;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // Enable pointerlock
            }
        }
    }
    
    // Clean up all resources before destroying the GLFW context
    cleanup_all_meshes();
    
    skybox.cleanup();
    
    // Clean up other OpenGL resources
    glUseProgram(0);
    if (renderer.shader.program != 0) {
        glDeleteProgram(renderer.shader.program);
    }
    
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

// ============================================================================
// USER INPUT
// ============================================================================

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    // Static variables to store previous mouse position and a flag for the first movement (center of window)
    static double lastX = 400.0;
    static double lastY = 300.0;
    static bool firstMouse = true; // Flag to handle the initial jump when pointer lock starts
    
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    
    // Calculate the offset from the last frame
    double xoffset = xpos - lastX;
    double yoffset = lastY - ypos; // Inverted Y-axis

    // Update lastX and lastY for the next frame
    lastX = xpos;
    lastY = ypos;

    if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED) return;

    // Apply sensitivity to mouse movement
    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;
    
    // Update camera's yaw and pitch
    global_camera.yaw += xoffset;
    global_camera.pitch += yoffset;

    // Constrain pitch to avoid flipping the camera upside down
    if (global_camera.pitch > 89.0f) {
        global_camera.pitch = 89.0f;
    }
    if (global_camera.pitch < -89.0f) {
        global_camera.pitch = -89.0f;
    }
    
    // After updating yaw/pitch, recalculate the camera's front, right, and up vectors
    camera_update_vectors(&global_camera);
}

// Callback for when the window is resized
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    // Adjust the OpenGL viewport to match the new window dimensions
    glViewport(0, 0, width, height);
    
    // Update camera aspect ratio
    if (height > 0) {
        global_camera.aspect_ratio = (float)width / (float)height;
    }
}

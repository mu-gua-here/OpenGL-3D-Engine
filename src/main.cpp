//
//  main.cpp
//  OpenGL 3D Engine
//
//  Created by mu-gua-here on 2025/9/15.
//

/*
 
 CC ATTRIBUTION
 3D MODELS:
 Realistic tree model: "Tree02" (https://free3d.com/3d-model/tree02-35663.html) by rezashams313 is licensed under Creative Commons Attribution-NonCommercial (http://creativecommons.org/licenses/by-nc/4.0/).
 Road track model: "Road Modular" (https://skfb.ly/pxYZw) by golukumar is licensed under Creative Commons Attribution (http://creativecommons.org/licenses/by/4.0/).
 Residential buildings model: "Low Poly Residential Buildings Pack" (https://free3d.com/3d-model/array-house-example-3033.html) by 3dhaupt is licensed under Creative Commons Attribution-NonCommercial (http://creativecommons.org/licenses/by-nc/4.0/).
 Lamppost model: "Street Lantern" (https://skfb.ly/oMIXI) by Samuel F. Johanns (Oneironauticus) is licensed under Creative Commons Attribution (http://creativecommons.org/licenses/by/4.0/). 
 
 TEXTURES:
 Checkered grass texture: "Green grass field pattern background for soccer and football. | Premium Photo" (https://ar.pinterest.com/pin/797629784037209282/) by Freepik is licensed under Creative Commons Attribution (http://creativecommons.org/licenses/by/4.0/).

 IMPLEMENTATION LIST
 1. Physics (maybe)
 2. Optimizations
    a. Batching
    b. Instancing
    c. Frustum culling
    d. LOD
 3. Complete PBR implementation (esp. IBL)
 4. Increase view distance for shadows
 
 BUGS & IMPROVEMENTS LIST
 1. Fix ImGui window mouse interaction
 2. Height map still not working - when height scale increases texture warps instead of increasing in depth
 3. Fix lamppost mesh

 NOTES FOR OTHER DEVELOPERS
 1. If you try to export the textures here elsewhere it might look strange because I'd flipped the textures for them to work in OpenGL

 */

// OpenGL-related
#include "glad/glad.h"
#include <GLFW/glfw3.h>

// ImGui
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// Texture loader
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// 3D file importer
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// C++ extensions
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_set>
#include <memory>
#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdio>

// Filesystem access
#include <filesystem>
#ifdef __APPLE__
    #include <mach-o/dyld.h>
#endif

// GLM library
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/euler_angles.hpp>

// Platform-specific includes for executable path
#ifdef _WIN32
    #include <windows.h>
    #ifndef MAX_PATH
        #define MAX_PATH 260
    #endif
#elif __linux__
    #include <unistd.h>
    #include <linux/limits.h>
    #ifndef PATH_MAX
        #define PATH_MAX 4096
    #endif
#elif __APPLE__
    // mach-o/dyld.h already included
    #include <limits.h>
    #ifndef PATH_MAX
        #define PATH_MAX 4096
    #endif
#endif

// Function prototypes
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// System
int WINDOW_WIDTH = 800;
int WINDOW_HEIGHT = 600;

std::filesystem::path executable_path;

// Runtime
bool firstMouse = true;
double fps;
bool paused = false;
float update_count = 0;
float frame_time = 1.0f;
float tick_speed = 1.0f;
bool fullscreen = false;

// Performance queries
GLuint shadowQueries[2] = {0, 0};
GLuint mainQueries[2] = {0, 0};
GLuint skyboxQueries[2] = {0, 0};
int queryIndex = 0;
int prevIndex = 0;
double shadowTime = 0.0;
double mainTime = 0.0;
double skyboxTime = 0.0;

// Player/camera
float cam_speed_multiplier = 0.005f;
glm::vec3 global_camera_vel = {0, 0, 0};
float friction = 0.9f;
glm::mat4 view;
glm::mat4 projection;

// Scene stats
unsigned int total_triangles = 0;
unsigned int entity_count = 0;

// Shadow mapping
GLuint shadowMapFBO = 0;
GLuint shadowMapTexture = 0;
unsigned int SHADOW_WIDTH = 4096;
unsigned int SHADOW_HEIGHT = 4096;
glm::mat4 lightSpaceMatrix;

// Game settings
#define MAX_LIGHTS 8
float mouse_sensitivity = 0.1f;

// Skybox
unsigned int skyboxID;

GLuint default_texture_id = 0;

const glm::vec3 VEC3_NO_CHANGE = glm::vec3(NAN, NAN, NAN);

// ============================================================================
// FPS COUNTER
// ============================================================================

void updateFPS(GLFWwindow* window) {
    static double lastTime = glfwGetTime();
    static unsigned int frameCount = 0;
    static double fpsTimer = 0.0;
    
    double currentTime = glfwGetTime();
    frame_time = currentTime - lastTime;
    lastTime = currentTime;
    tick_speed = frame_time / 0.0083f;
    
    frameCount++;
    fpsTimer += frame_time;
    
    if (fpsTimer >= 1.0) {
        // Update fps counter
        fps = frameCount / fpsTimer;
        char title[256];
        snprintf(title, sizeof(title), "OpenGL 3D Engine by @mu-gua-here");
        glfwSetWindowTitle(window, title);
        frameCount = 0;
        fpsTimer = 0.0;

        // Update performance query results
        GLuint64 shadowTimeNS = 0;
        glGetQueryObjectui64v(shadowQueries[prevIndex], GL_QUERY_RESULT, &shadowTimeNS);
        shadowTime = shadowTimeNS / 1000000.0;

        GLuint64 skyboxTimeNS = 0;
        glGetQueryObjectui64v(skyboxQueries[prevIndex], GL_QUERY_RESULT, &skyboxTimeNS);
        skyboxTime = skyboxTimeNS / 1000000.0;

        GLuint64 mainTimeNS = 0;
        glGetQueryObjectui64v(mainQueries[prevIndex], GL_QUERY_RESULT, &mainTimeNS);
        mainTime = mainTimeNS / 1000000.0;
    }
}

// ============================================================================
// ASSET PATH BUILDING
// ============================================================================

std::filesystem::path getExecutableDirectory() {
    std::filesystem::path execPath;
    
#ifdef _WIN32
    // Windows
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    execPath = std::filesystem::path(buffer).parent_path();
    
#elif __APPLE__
    // macOS
    char buffer[PATH_MAX];
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) == 0) {
        execPath = std::filesystem::path(buffer).parent_path();
    } else {
        printf("Warning: Buffer too small for executable path\n");
        execPath = std::filesystem::current_path();
    }
    
#elif __linux__
    // Linux
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1) {
        buffer[len] = '\0';
        execPath = std::filesystem::path(buffer).parent_path();
    } else {
        printf("Warning: Could not read executable path\n");
        execPath = std::filesystem::current_path();
    }
    
#else
    // Fallback for unknown platforms
    execPath = std::filesystem::current_path();
#endif
    
    printf("Executable directory: %s\n", execPath.string().c_str());
    return execPath;
}

std::string getExecutablePath() {
    executable_path = getExecutableDirectory();
    
    // Check if we're in a development environment (has CMakeLists.txt in parent dirs)
    std::filesystem::path searchPath = executable_path;
    bool isDevEnvironment = false;
    
    // Search up to 5 levels up for CMakeLists.txt (development indicator)
    for (int i = 0; i < 5; i++) {
        if (std::filesystem::exists(searchPath / "CMakeLists.txt")) {
            isDevEnvironment = true;
            executable_path = searchPath; // Use project root
            printf("Development environment detected, using project root: %s\n", searchPath.string().c_str());
            break;
        }
        if (searchPath.has_parent_path()) {
            searchPath = searchPath.parent_path();
        } else {
            break;
        }
    }
    
    if (!isDevEnvironment) {
        printf("Deployed environment detected, using executable directory\n");
    }
    
    printf("Asset base path: %s\n", executable_path.string().c_str());
    return executable_path.string();
}

std::string buildAssetPath(const std::string& relative_path) {
    std::filesystem::path fullPath = executable_path / relative_path;
    
    // Verify the path exists
    if (!std::filesystem::exists(fullPath)) {
        printf("Warning: Asset path does not exist: %s\n", fullPath.string().c_str());
    }
    
    return fullPath.string();
}

std::string resolveTexturePath(const std::string& modelPath, const std::string& textureName) {
    std::filesystem::path modelDir = std::filesystem::path(modelPath).parent_path();
    std::filesystem::path fullPath = modelDir / textureName;
    return "scene_models/" + fullPath.string();
}

// ============================================================================
// MATERIAL SYSTEM
// ============================================================================

class Color {
public:
    float r, g, b, a;
    
    Color() : r(0.0f), g(0.0f), b(0.0f), a(1.0f) {}
    Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}
    
    Color operator*(float scalar) const {
        return Color(r * scalar, g * scalar, b * scalar, a);
    }
    
    glm::vec4 toVec4() const { return glm::vec4(r, g, b, a); }
};

// Material class
class Material {
public:
    std::string name;
    
    // PBR properties with proper defaults
    glm::vec3 base_color{1.0f, 1.0f, 1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float ao = 1.0f;
    glm::vec3 emissive{0.0f, 0.0f, 0.0f};
    
    GLuint albedo_map = 0;
    GLuint normal_map = 0;
    GLuint orm_map = 0; // AO, roughness, metallic and height packed
    GLuint height_map = 0;
    GLuint emissive_map = 0;
    GLuint specular_map = 0;
    
    float height_scale = 0.01f;
    
    Material() = default;
    Material(const std::string& name) : name(name) {}
    
    bool hasAlbedoMap() const { return albedo_map != 0; }
    bool hasNormalMap() const { return normal_map != 0; }
    bool hasORMMap() const { return orm_map != 0; }
    bool hasEmissiveMap() const { return emissive_map != 0; }
    bool hasHeightMap() const { return height_map != 0; }
    bool hasSpecularMap() const { return specular_map != 0; }
};

Material createDefaultMaterial() {
    Material default_mat;
    default_mat.name = "default";
    default_mat.albedo_map = default_texture_id;
    default_mat.base_color = {1.0f, 1.0f, 1.0f};
    default_mat.metallic = 0.0f;
    default_mat.roughness = 0.5f;
    default_mat.ao = 1.0f;
    return default_mat;
}

// ============================================================================
// TEXTURE LOADING
// ============================================================================

GLuint createDefaultTexture() {
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    
    const unsigned char white_pixel[4] = {255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white_pixel);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture_id;
}

GLuint loadTexture(const std::string& path) {
    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
    if (!data) {
        printf("Failed to load texture: %s\n", path.c_str());
        return default_texture_id;
    }
    
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    GLenum internalFormat, dataFormat;
    if (channels == 1) {
        internalFormat = GL_R8;
        dataFormat = GL_RED;
    } else if (channels == 3) {
        internalFormat = GL_RGB8;
        dataFormat = GL_RGB;
    } else if (channels == 4) {
        internalFormat = GL_RGBA8;
        dataFormat = GL_RGBA;
    } else {
        stbi_image_free(data);
        return default_texture_id;
    }
    
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    printf("Successfully loaded texture: %s (%dx%d, %d channels)\n", path.c_str(), width, height, channels);
    return textureID;
}

GLuint loadTextureFromMemory(unsigned char* data, unsigned int size) {
    int width, height, channels;
    unsigned char* image = stbi_load_from_memory(data, size, &width, &height, &channels, 0);
    if (!image) {
        printf("Failed to decode embedded texture\n");
        return default_texture_id;
    }
    
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, image);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    stbi_image_free(image);
    return textureID;
}

GLuint loadTextureFromARGB(aiTexel* data, unsigned int width, unsigned int height) {
    if (!data || width == 0 || height == 0) {
        printf("Invalid ARGB texture data\n");
        return default_texture_id;
    }
    
    // aiTexel is a struct with b, g, r, a components (each unsigned char)
    // We need to convert ARGB to RGBA for OpenGL
    size_t dataSize = width * height * 4;
    unsigned char* rgbaData = new unsigned char[dataSize];
    
    for (unsigned int i = 0; i < width * height; ++i) {
        rgbaData[i * 4 + 0] = data[i].r;  // R
        rgbaData[i * 4 + 1] = data[i].g;  // G
        rgbaData[i * 4 + 2] = data[i].b;  // B
        rgbaData[i * 4 + 3] = data[i].a;  // A
    }
    
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaData);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    delete[] rgbaData;
    glBindTexture(GL_TEXTURE_2D, 0);
    
    printf("Successfully loaded embedded ARGB texture (%ux%u)\n", width, height);
    return textureID;
}

GLuint loadCubemap(const char* faces[6]) {
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, channels;
    for (unsigned int i = 0; i < 6; i++) {
        unsigned char* data = stbi_load(faces[i], &width, &height, &channels, 0);
        if (!data) {
            printf("Failed to load cubemap texture piece: %s\n", faces[i]);
            return default_texture_id;
        }
        GLenum internalFormat, dataFormat;
        if (channels == 1) {
            internalFormat = GL_R8;
            dataFormat = GL_RED;
        } else if (channels == 3) {
            internalFormat = GL_RGB8;
            dataFormat = GL_RGB;
        } else if (channels == 4) {
            internalFormat = GL_RGBA8;
            dataFormat = GL_RGBA;
        } else {
            stbi_image_free(data);
            return default_texture_id;
        }
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
        printf("Successfully loaded cubemap texture piece: %s (%dx%d, %d channels)\n", faces[i], width, height, channels);
    }
    
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    
    return textureID;
}

// ============================================================================
// SHADOW MAPPING
// ============================================================================

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

// ============================================================================
// MESH SYSTEM
// ============================================================================

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
    GLuint VAO, VBO, EBO;
    Material material;
    int cull_mode;
    bool is_cleaned_up;
    
    Mesh() : TRIANGLE_COUNT(0), VAO(0), VBO(0), EBO(0), cull_mode(CULL_NONE), is_cleaned_up(false) {
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
        
        TRIANGLE_COUNT = INDEX_COUNT = 0;
        is_cleaned_up = true;
    }
};

// ============================================================================
// ENTITY SYSTEM
// ============================================================================

struct Entity {
    std::string name;
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
    std::vector<std::unique_ptr<Mesh>> meshes;
    int active;
};

class EntityManager {
private:
    std::vector<Entity> entities;
    
public:
    size_t addEntity(Entity&& entity) {
        entities.push_back(std::move(entity));
        return entities.size() - 1;
    }

    int findEntity(std::string name) {
        for (size_t i = 0; i < entities.size(); i++) {
            if (entities[i].active && entities[i].name == name) {
                return i;
            }
        }
        return -1;
    }
    
    bool updateEntity(std::string name, const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scale) {
        int i = findEntity(name);
        if (i == -1) return false;

        // Positions
        if (!isnan(pos.x)) entities[i].position.x = pos.x;
        if (!isnan(pos.y)) entities[i].position.y = pos.y;
        if (!isnan(pos.z)) entities[i].position.z = pos.z;
        
        // Rotations
        if (!isnan(rot.x)) entities[i].rotation.x = rot.x;
        if (!isnan(rot.y)) entities[i].rotation.y = rot.y;
        if (!isnan(rot.z)) entities[i].rotation.z = rot.z;
        
        // Scale
        if (!isnan(scale.x)) entities[i].scale.x = scale.x;
        if (!isnan(scale.y)) entities[i].scale.y = scale.y;
        if (!isnan(scale.z)) entities[i].scale.z = scale.z;

        return true;
    }

    void updateEntity(size_t index, const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scale) {        
        // Positions
        if (!isnan(pos.x)) entities[index].position.x = pos.x;
        if (!isnan(pos.y)) entities[index].position.y = pos.y;
        if (!isnan(pos.z)) entities[index].position.z = pos.z;
        
        // Rotations
        if (!isnan(rot.x)) entities[index].rotation.x = rot.x;
        if (!isnan(rot.y)) entities[index].rotation.y = rot.y;
        if (!isnan(rot.z)) entities[index].rotation.z = rot.z;
        
        // Scale
        if (!isnan(scale.x)) entities[index].scale.x = scale.x;
        if (!isnan(scale.y)) entities[index].scale.y = scale.y;
        if (!isnan(scale.z)) entities[index].scale.z = scale.z;
    }
    
    size_t size() const { return entities.size(); }
    
    Entity* getEntityAt(size_t index) {
        if (index < entities.size() && entities[index].active) {
            return &entities[index];
        }
        return nullptr;
    }
    
    template <typename Pred>
    void removeEntities(Pred&& pred) {
        for (Entity& entity : entities) {
            if (entity.active && pred(entity)) {
                for (const auto& mesh : entity.meshes) total_triangles -= mesh->TRIANGLE_COUNT;
                entity.meshes.clear();
                entity.active = false;
            }
        }
    }
};

EntityManager entity_manager;

template<typename... Args>
void createEntity(std::string name, std::vector<std::unique_ptr<Mesh>>&& meshes, glm::vec3 pos, glm::vec3 rotation, glm::vec3 scale, std::vector<int> cull_modes) {
    Entity entity;
    entity.name = name;
    entity.position = pos;
    entity.rotation = rotation;
    entity.scale = scale;
    entity.active = 1;
    
    // Apply cull modes for individual submeshes
    for (size_t i = 0; i < meshes.size(); ++i) {
        if (meshes[i]) {
            if (i < cull_modes.size()) {
                // If specified use the given cull mode, otherwise use CULL_NONE
                if (cull_modes[i]) {
                    meshes[i]->cull_mode = cull_modes[i];
                } else {
                    meshes[i]->cull_mode = CULL_NONE;
                }
            }
        }
    }
    
    // Count triangles then move memory address
    unsigned int mesh_triangles = 0;
    for (const auto& mesh : meshes) {
        if (mesh) {
            mesh_triangles += mesh->TRIANGLE_COUNT;
        }
    }
    total_triangles += mesh_triangles;
    
    printf("Created entity '%s' with %zu submeshes (triangles: %u)\n",
           name.c_str(), meshes.size(), mesh_triangles);
    
    entity.meshes = std::move(meshes);
    entity_manager.addEntity(std::move(entity));
}

// ============================================================================
// CAMERA SYSTEM
// ============================================================================

typedef struct {
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    float yaw;
    float pitch;
    float fov;
    float aspect_ratio;
    float near_plane;
    float far_plane;
} Camera;

Camera global_camera;

Camera create_camera(float aspect) {
    Camera cam;
    cam.position = glm::vec3(0, 2, 9);
    cam.front = glm::vec3(0, 0, -1);
    cam.up = glm::vec3(0, 1, 0);
    cam.right = glm::vec3(1, 0, 0);
    cam.yaw = -90.0f;
    cam.pitch = 0.0f;
    cam.fov = glm::radians(45.0f);
    cam.aspect_ratio = aspect;
    cam.near_plane = 0.1f;
    cam.far_plane = 200.0f;
    return cam;
}

void camera_update_vectors(Camera* cam) {
    cam->front.x = cos(glm::radians(cam->yaw)) * cos(glm::radians(cam->pitch));
    cam->front.y = sin(glm::radians(cam->pitch));
    cam->front.z = sin(glm::radians(cam->yaw)) * cos(glm::radians(cam->pitch));
    cam->front = glm::normalize(cam->front);

    cam->right = glm::normalize(glm::cross(cam->front, glm::vec3(0, 1, 0)));
    cam->up = glm::normalize(glm::cross(cam->right, cam->front));
}

glm::mat4 camera_get_projection(Camera* cam) {
    return glm::perspective(cam->fov, cam->aspect_ratio, cam->near_plane, cam->far_plane);
}

glm::mat4 camera_get_view_matrix(Camera* cam) {
    return glm::lookAt(cam->position, cam->position + cam->front, cam->up);
}

// ============================================================================
// LIGHTING SYSTEM
// ============================================================================

// Add an enum to classify light types
typedef enum {
    DIR_LIGHT = 0,
    POINT_LIGHT = 1,
    SPOT_LIGHT = 2,
} LightType;

typedef struct {
    glm::vec3 position;
    glm::vec3 color;
    int intensity;
    
    // Spotlight-specific parameters
    glm::vec3 direction;
    float inner_cutoff_cos;
    float outer_cutoff_cos;
    int type;
    
    std::string entity_name;
} Light;

std::vector<Light> lights;

glm::vec3 convertVecToEuler(glm::vec3 direction, glm::vec3 offset) {
    glm::mat4 rotationMatrix = glm::inverse(glm::lookAt(glm::vec3(0, 0, 0), direction, glm::vec3(0.0f, 1.0f, 0.0f)));
    float yaw, pitch, roll;
    glm::extractEulerAngleZYX(rotationMatrix, yaw, pitch, roll);
    glm::vec3 euler_rot = glm::vec3(glm::degrees(roll) + offset.x, glm::degrees(pitch) + offset.y, glm::degrees(yaw) + offset.z);
    return euler_rot;
}

void createDirLight(std::string name, glm::vec3 direction, glm::vec3 color, int intensity) {
    Light light;
    light.color = color;
    light.intensity = intensity;
    light.direction = glm::normalize(direction);
    // Set spotlight-only settings to invalid
    light.inner_cutoff_cos = -1.0f;
    light.outer_cutoff_cos = -1.0f;
    light.type = DIR_LIGHT;
    light.entity_name = name;

    if (lights.empty()) {
        SHADOW_WIDTH = SHADOW_HEIGHT = 16384;
    }

    if (lights.size() >= MAX_LIGHTS) {
        printf("Warning: Exceeded maximum number of lights (%d). Additional lights will be ignored in shaders.\n", MAX_LIGHTS);
        return;
    }

    lights.push_back(light);
}

void createSpotlight(std::string name, glm::vec3 position, glm::vec3 color, int intensity,
                     glm::vec3 direction, float inner_angle_deg, float outer_angle_deg,
                     std::vector<std::unique_ptr<Mesh>>&& light_mesh, glm::vec3 scale, std::vector<int> cull_mode) {
    Light light;
    light.position = position;
    light.color = color;
    light.intensity = intensity;
    light.direction = glm::normalize(direction);
    light.inner_cutoff_cos = glm::cos(glm::radians(inner_angle_deg));
    light.outer_cutoff_cos = glm::cos(glm::radians(outer_angle_deg));
    light.type = SPOT_LIGHT;
    light.entity_name = name;

    if (lights.empty()) {
        SHADOW_WIDTH = SHADOW_HEIGHT = 4096;
    }

    if (lights.size() >= MAX_LIGHTS) {
        printf("Warning: Exceeded maximum number of lights (%d). Additional lights will be ignored in shaders.\n", MAX_LIGHTS);
        return;
    }

    lights.push_back(light);
    glm::vec3 rotation = convertVecToEuler(light.direction, glm::vec3(0.0f));

    if (!light_mesh.empty()) createEntity(light.entity_name, std::move(light_mesh), position, rotation, scale, cull_mode);
}

void createPointLight(std::string name, glm::vec3 position, glm::vec3 color, int intensity,
                      std::vector<std::unique_ptr<Mesh>>&& light_mesh, glm::vec3 scale, std::vector<int> cull_mode) {
    Light light;
    light.position = position;
    light.color = color;
    light.intensity = intensity;
    
    // Set spotlight-only settings to invalid
    light.direction = glm::vec3(0.0f, -1.0f, 0.0f);
    light.inner_cutoff_cos = -1.0f;
    light.outer_cutoff_cos = -1.0f;
    light.type = POINT_LIGHT;
    light.entity_name = name;
    
    if (lights.empty()) {
        SHADOW_WIDTH = SHADOW_HEIGHT = 8192;
    }

    if (lights.size() > MAX_LIGHTS) {
        printf("Warning: Exceeded maximum number of lights (%d). Additional lights will be ignored in shaders.\n", MAX_LIGHTS);
        return;
    }

    lights.push_back(light);

    if (!light_mesh.empty()) createEntity(light.entity_name, std::move(light_mesh), position, light.direction, scale, cull_mode);
}

void updateLight(std::string name, glm::vec3 position, glm::vec3 color, int intensity, glm::vec3 rotation, glm::vec3 offset) {
    int lightIndex = -1;
    for (size_t i = 0; i < lights.size(); i++) {
        if (lights[i].entity_name == name) {
            lightIndex = i;
            break;
        }
    }
    
    if (lightIndex == -1) {
        printf("Warning: Light '%s' not found\n", name.c_str());
        return;
    }
    
    if (!isnan(position.x)) lights[lightIndex].position.x = position.x;
    if (!isnan(position.y)) lights[lightIndex].position.y = position.y;
    if (!isnan(position.z)) lights[lightIndex].position.z = position.z;
    
    if (!isnan(color.r)) lights[lightIndex].color.r = color.r;
    if (!isnan(color.g)) lights[lightIndex].color.g = color.g;
    if (!isnan(color.b)) lights[lightIndex].color.b = color.b;
    
    if (intensity >= 0) lights[lightIndex].intensity = intensity;

    if (!isnan(rotation.x) && !isnan(rotation.y) && !isnan(rotation.z)) {
        lights[lightIndex].direction = glm::normalize(rotation);
        entity_manager.updateEntity(name, position, convertVecToEuler(rotation, offset), VEC3_NO_CHANGE);
    }
}

// ============================================================================
// SHADER CLASS
// ============================================================================

class Shader {
private:
    GLuint program_id = 0;
    mutable std::unordered_map<std::string, GLint> uniform_cache;
    
public:
    Shader(const std::string& vertex_source, const std::string& fragment_source) {
        program_id = createShaderProgram(vertex_source, fragment_source);
        if (program_id == 0) {
            throw std::runtime_error("Failed to create shader program");
        }
    }
    
    ~Shader() {
        if (program_id != 0) {
            glDeleteProgram(program_id);
        }
    }
    
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    
    void use() const {
        glUseProgram(program_id);
    }
    
    GLuint getProgram() const { return program_id; }
    
    GLint getUniformLocation(const std::string& name) const {
        if (auto it = uniform_cache.find(name); it != uniform_cache.end()) {
            return it->second;
        }
        
        GLint location = glGetUniformLocation(program_id, name.c_str());
        uniform_cache[name] = location;
        
        if (location == -1) {
            printf("Warning: Uniform '%s' not found in shader\n", name.c_str());
        }
        
        return location;
    }
    
    void setMat4(const std::string& name, const glm::mat4& value) const {
        glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
    }
    
    void setMat3(const std::string& name, const glm::mat3& value) const {
        glUniformMatrix3fv(getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
    }
    
    void setVec3(const std::string& name, const glm::vec3& value) const {
        glUniform3fv(getUniformLocation(name), 1, glm::value_ptr(value));
    }
    
    void setInt(const std::string& name, int value) const {
        glUniform1i(getUniformLocation(name), value);
    }
    
    void setFloat(const std::string& name, float value) const {
        glUniform1f(getUniformLocation(name), value);
    }
    
    void setVec3Array(const std::string& name, const std::vector<glm::vec3>& values) const {
        glUniform3fv(getUniformLocation(name), static_cast<GLsizei>(values.size()),
                     reinterpret_cast<const GLfloat*>(values.data()));
    }
    
    void setFloatArray(const std::string& name, const std::vector<float>& values) const {
        glUniform1fv(getUniformLocation(name), static_cast<GLsizei>(values.size()), values.data());
    }

private:
    GLuint createShaderProgram(const std::string& vertex_source, const std::string& fragment_source) {
        GLuint vertex_shader = compileShader(GL_VERTEX_SHADER, vertex_source);
        if (vertex_shader == 0) return 0;
        
        GLuint fragment_shader = compileShader(GL_FRAGMENT_SHADER, fragment_source);
        if (fragment_shader == 0) {
            glDeleteShader(vertex_shader);
            return 0;
        }
        
        GLuint program = linkShaderProgram(vertex_shader, fragment_shader);
        
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        
        return program;
    }
    
    GLuint compileShader(GLenum shader_type, const std::string& source) {
        GLuint shader = glCreateShader(shader_type);
        const char* source_cstr = source.c_str();
        glShaderSource(shader, 1, &source_cstr, nullptr);
        glCompileShader(shader);
        
        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            GLchar info_log[512];
            glGetShaderInfoLog(shader, 512, nullptr, info_log);
            printf("Shader compilation failed (%s): %s\n",
                   (shader_type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT",
                   info_log);
            glDeleteShader(shader);
            return 0;
        }
        
        return shader;
    }
    
    GLuint linkShaderProgram(GLuint vertex_shader, GLuint fragment_shader) {
        GLuint program = glCreateProgram();
        glAttachShader(program, vertex_shader);
        glAttachShader(program, fragment_shader);
        glLinkProgram(program);
        
        GLint success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            GLchar info_log[512];
            glGetProgramInfoLog(program, 512, nullptr, info_log);
            printf("Shader program linking failed: %s\n", info_log);
            glDeleteProgram(program);
            return 0;
        }
        
        return program;
    }
};

// ============================================================================
// PBR VERTEX SHADER
// ============================================================================

const std::string pbr_vertex_shader = R"(
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aNormal;
layout (location = 4) in vec3 aTangent;
layout (location = 5) in vec3 aBitangent;

out vec4 vertexColor;
out vec2 TexCoord;
out vec3 FragPos;
out vec3 Normal; // Normal in world space
out vec4 FragPosLightSpace;
out mat3 TBN; // Tangent-Bitangent-Normal matrix in world space

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix; // Inverse transpose of the model matrix
uniform mat4 lightSpaceMatrix;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = normalMatrix * aNormal; // Use normalMatrix for correct Normal
    gl_Position = projection * view * vec4(FragPos, 1.0);
    
    TexCoord = aTexCoords;
    vertexColor = aColor;
    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);
    
    // Construct TBN matrix
    vec3 T = normalize(normalMatrix * aTangent);
    vec3 B = normalize(normalMatrix * aBitangent);
    vec3 N_TBN = normalize(Normal); // Use the already calculated world space normal
    
    TBN = mat3(T, B, N_TBN);
}
)";

// ============================================================================
// PBR FRAGMENT SHADER (Cook-Torrance BRDF)
// ============================================================================

const std::string pbr_fragment_shader = R"(
#version 330 core
in vec4 vertexColor;
in vec2 TexCoord;
in vec3 FragPos;
in vec3 Normal;
in vec4 FragPosLightSpace;
in mat3 TBN;

out vec4 FragColor;

// Samplers
uniform sampler2D albedoMap;
uniform sampler2D normalMap;
uniform sampler2D ormMap;
uniform sampler2D emissiveMap;
uniform sampler2DShadow shadowMap;

// Material scalar properties
uniform vec3 baseColor;
uniform float metallic;
uniform float roughness;
uniform float ao;
uniform vec3 emissive;
uniform float heightScale;

uniform bool hasAlbedoMap;
uniform bool hasNormalMap;
uniform bool hasEmissiveMap;
uniform bool hasORMMap;
uniform bool hasHeightMap;

// Lighting
#define MAX_LIGHTS 8
uniform int lightCount;
uniform vec3 viewPos;
uniform int shadowLightIndex;
uniform mat4 lightSpaceMatrix;

uniform vec3 lightPositions[MAX_LIGHTS];
uniform vec3 lightColors[MAX_LIGHTS];
uniform float lightIntensities[MAX_LIGHTS];
uniform vec3 lightDirections[MAX_LIGHTS];
uniform float lightInnerCutoffs[MAX_LIGHTS];
uniform float lightOuterCutoffs[MAX_LIGHTS];
uniform float lightTypes[MAX_LIGHTS];

const float PI = 3.14159265359;

// PARALLAX OCCLUSION MAPPING
vec2 parallaxMapping(vec2 texCoords, vec3 viewDir) { 
    const float minLayers = 8.0;
    const float maxLayers = 32.0;
    float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 1.0), viewDir)));  
    float layerDepth = 1.0 / numLayers;
    float currentLayerDepth = 0.0;
    
    vec2 P = viewDir.xy / viewDir.z * heightScale;
    vec2 deltaTexCoords = P / numLayers;
  
    vec2 currentTexCoords = texCoords;
    float currentDepthMapValue = texture(ormMap, currentTexCoords).a;
      
    while (currentLayerDepth < currentDepthMapValue) {
        currentTexCoords -= deltaTexCoords;
        currentDepthMapValue = texture(ormMap, currentTexCoords).a;  
        currentLayerDepth += layerDepth;  
    }
    
    vec2 prevTexCoords = currentTexCoords + deltaTexCoords;
    float afterDepth = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = texture(ormMap, prevTexCoords).a - currentLayerDepth + layerDepth;
 
    float weight = afterDepth / (afterDepth - beforeDepth);
    vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

    return finalTexCoords;
}

// SHADOW MAPPING
float calcShadow(vec4 fragLight, vec3 N, vec3 L) {
    if (shadowLightIndex < 0) return 0.0;

    // Apply normal offset before projection to prevent acne
    vec3 offsetPos = FragPos + N * 0.1;
    vec4 offsetLight = lightSpaceMatrix * vec4(offsetPos, 1.0);
    
    vec3 proj = offsetLight.xyz / offsetLight.w;
    proj = proj * 0.5 + 0.5;

    if (proj.z > 1.0) return 0.0;

    // Minimal bias needed since we use normal offset
    float cosTheta = max(dot(N, L), 0.0);
    float bias = 0.0001 * (1.0 - cosTheta);

    // Poisson disk sampling offsets
    vec2 poissonDisk[16] = vec2[](
        vec2(-0.94201624, -0.39906216),
        vec2(0.94558609, -0.76890725),
        vec2(-0.094184101, -0.92938870),
        vec2(0.34495938, 0.29387760),
        vec2(-0.91588581, 0.45771432),
        vec2(-0.81544232, -0.87912464),
        vec2(-0.38277543, 0.27676845),
        vec2(0.97484398, 0.75648379),
        vec2(0.44323325, -0.97511554),
        vec2(0.53742981, -0.47373420),
        vec2(-0.26496911, -0.41893023),
        vec2(0.79197514, 0.19090188),
        vec2(-0.24188840, 0.99706507),
        vec2(-0.81409955, 0.91437590),
        vec2(0.19984126, 0.78641367),
        vec2(0.14383161, -0.14100790)
    );

    // PCF sampling with Poisson disk distribution
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    
    // Adjust the multiplier to control shadow softness (higher = softer)
    float spread = 2.0;
    
    for (int i = 0; i < 16; ++i) {
        vec2 offset = poissonDisk[i] * texelSize * spread;
        float shadowSample = texture(shadowMap, vec3(proj.xy + offset, proj.z - bias));
        shadow += shadowSample; // Returns 0.0 (in shadow) or 1.0 (lit)
    }
    shadow /= 16.0;

    // Fade shadows at edges
    vec2 border = min(proj.xy, 1.0 - proj.xy);
    float fade = smoothstep(0.0, 0.1, min(border.x, border.y));
    
    // Invert because sampler2DShadow returns 1.0 for lit, 0.0 for shadow
    return mix(0.0, 1.0 - shadow, fade);
}

// PBR FUNCTIONS
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float D_GGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    denom = PI * denom * denom;
    
    return a2 / max(denom, 0.0001);
}

float G_SchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    
    float denom = NdotV * (1.0 - k) + k;
    
    return NdotV / max(denom, 0.0001);
}

float G_Smith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = G_SchlickGGX(NdotV, roughness);
    float ggx1 = G_SchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

// MAIN
void main() {
    // Calculate view direction in tangent space for POM
    vec3 Vworld = normalize(viewPos - FragPos);
    vec2 uv = TexCoord;
    
    if (hasHeightMap) {
        vec3 Vts = normalize(transpose(TBN) * Vworld);
        if (abs(Vts.z) < 0.001) Vts.z = 0.001;
        uv = parallaxMapping(TexCoord, Vts);
    }
    
    // Sample material properties
    vec4 albedoSample = hasAlbedoMap ? texture(albedoMap, uv) : vec4(baseColor, 1.0);
    if (albedoSample.a < 0.5) discard;
    
    vec3 albedo = albedoSample.rgb;
    
    // Sample or use scalar values for ORM
    float aoValue = ao;
    float roughValue = roughness;
    float metalValue = metallic;
    
    if (hasORMMap) {
        vec3 ormSample = texture(ormMap, uv).rgb;
        aoValue = ormSample.r;
        roughValue = ormSample.g;
        metalValue = ormSample.b;
    }
    
    // Clamp values to valid ranges
    roughValue = clamp(roughValue, 0.04, 1.0);  // Minimum roughness to avoid artifacts
    metalValue = clamp(metalValue, 0.0, 1.0);
    aoValue = clamp(aoValue, 0.0, 1.0);
    
    vec3 emissiveCol = hasEmissiveMap ? texture(emissiveMap, uv).rgb : emissive;
    
    // Calculate normal
    vec3 N = normalize(Normal);
    if (hasNormalMap) {
        vec3 nt = texture(normalMap, uv).rgb * 2.0 - 1.0;
        N = normalize(TBN * nt);
    }
    
    // Flip normal if facing away
    if (!gl_FrontFacing) N = -N;
    
    vec3 V = normalize(Vworld);
    
    // Calculate F0 (base reflectivity)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metalValue);
    
    // Lighting calculation
    vec3 Lo = vec3(0.0);
    
    for (int i = 0; i < lightCount && i < MAX_LIGHTS; ++i) {
        vec3 L;
        float attenuation = 1.0;
        
        // Calculate light direction and attenuation
        if (lightTypes[i] == 0.0) {
            // Directional light
            L = normalize(-lightDirections[i]);
        } else {
            // Point or spot light
            L = normalize(lightPositions[i] - FragPos);
            float distance = length(lightPositions[i] - FragPos);
            attenuation = 1.0 / (distance * distance);
            
            // Spotlight attenuation
            if (lightTypes[i] == 2.0) {
                float theta = dot(L, normalize(-lightDirections[i]));
                float epsilon = lightInnerCutoffs[i] - lightOuterCutoffs[i];
                float intensity = clamp((theta - lightOuterCutoffs[i]) / epsilon, 0.0, 1.0);
                attenuation *= intensity;
            }
        }
        
        vec3 H = normalize(V + L);
        vec3 radiance = lightColors[i] * lightIntensities[i] * attenuation;
        
        // Cook-Torrance BRDF
        float NDF = D_GGX(N, H, roughValue);
        float G = G_Smith(N, V, L, roughValue);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metalValue;
        
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;
        
        // Shadow calculation
        float shadow = (i == shadowLightIndex) ? calcShadow(FragPosLightSpace, N, L) : 0.0;
        
        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL * (1.0 - shadow);
    }
    
    // Ambient lighting with AO
    vec3 ambient = vec3(0.3) * albedo * aoValue;
    vec3 color = ambient + Lo + emissiveCol;
    
    // Tone mapping (Reinhard)
    color = color / (color + vec3(1.0));
    
    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));
    
    FragColor = vec4(color, 1.0);
}
)";

// ============================================================================
// UNLIT SHADERS
// ============================================================================

const std::string unlit_vertex_shader = R"(
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

const std::string unlit_fragment_shader = R"(
#version 330 core
out vec4 FragColor;

uniform vec3 emissiveColor;
uniform float emissiveIntensity;

void main() {
    FragColor = vec4(emissiveColor * emissiveIntensity, 1.0);
}
)";

// ============================================================================
// SKYBOX SHADERS
// ============================================================================

const std::string skybox_vertex_shader = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
out vec3 TexCoords;
uniform mat4 projection;
uniform mat4 view;
void main() {
    TexCoords = aPos;
    vec4 pos = projection * view * vec4(aPos, 1.0);
    gl_Position = pos.xyww; // Make skybox always at far plane
}
)";

const std::string skybox_fragment_shader = R"(
#version 330 core
out vec4 FragColor;
in vec3 TexCoords;
uniform samplerCube skybox;
void main() {
    FragColor = texture(skybox, TexCoords);
}
)";

// ============================================================================
// SHADOWMAP SHADERS
// ============================================================================

const std::string shadow_vertex_shader = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 2) in vec2 aTexCoords;

uniform mat4 lightSpaceMatrix;
uniform mat4 model;

out vec2 TexCoord;

void main() {
    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
    TexCoord = aTexCoords;
}
)";

const std::string shadow_fragment_shader = R"(
#version 330 core

in vec2 TexCoord;

uniform sampler2D u_texture;

void main() {
    vec4 texColor = texture(u_texture, TexCoord);
    if (texColor.a < 0.5) discard; // Prevents the fragment from writing to the depth buffer
    // Depth is automatically written
}
)";

// ============================================================================
// SKYBOX CLASS
// ============================================================================

static const float SKYBOX_VERTICES[] = {
    // Positions
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

class Skybox {
public:
    GLuint VAO, VBO;
    std::vector<GLuint> cubemap_texture;
    std::unique_ptr<Shader> skybox_shader;
    
    Skybox() : VAO(0), VBO(0), cubemap_texture(0) {}
    
    ~Skybox() {
        cleanup();
    }
    
    void bindSkybox(const char* faces[6]) {
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
    
    void initShader() {
        try {
            skybox_shader = std::make_unique<Shader>(skybox_vertex_shader, skybox_fragment_shader);
            printf("Skybox shaders created successfully. %u\n", skybox_shader->getProgram());
        } catch (const std::exception& e) {
            printf("Failed to create skybox shaders: %s\n", e.what());
            throw;
        }
    }
    
    void render(Camera* camera) {
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
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap_texture[skyboxID]);
        skybox_shader->setInt("skybox", 0);
        
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);
    }
    
    void cleanup() {
        if (VAO != 0) glDeleteVertexArrays(1, &VAO);
        if (VBO != 0) glDeleteBuffers(1, &VBO);
        for (GLuint tex : cubemap_texture) {
            if (tex != 0) glDeleteTextures(1, &tex);
        }
        cubemap_texture.clear();
        VAO = VBO = 0;
    }
};

// ============================================================================
// RENDERER CLASS
// ============================================================================

class Renderer {
private:
    std::unique_ptr<Shader> pbr_shader;
    std::unique_ptr<Shader> shadow_shader;
    std::unique_ptr<Shader> unlit_shader;
    
public:
    Renderer() {
        try {
            pbr_shader = std::make_unique<Shader>(pbr_vertex_shader, pbr_fragment_shader);
            shadow_shader = std::make_unique<Shader>(shadow_vertex_shader, shadow_fragment_shader);
            unlit_shader = std::make_unique<Shader>(unlit_vertex_shader, unlit_fragment_shader);
            printf("Shaders created successfully. Main: %u, Shadow: %u, Unlit: %u\n",
                   pbr_shader->getProgram(), shadow_shader->getProgram(), unlit_shader->getProgram());
        } catch (const std::exception& e) {
            printf("Failed to create shaders: %s\n", e.what());
            throw;
        }
    }
    
    void renderShadowPass(EntityManager& entity_manager, const Light& light) {
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
    
    void setGlobalUniforms(const Camera& camera) {
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
    
    void drawEntity(Entity* entity, const Camera& camera, int shadowLightIndex) {
        if (!entity->active) return;
        for (const auto& meshPtr : entity->meshes) {
            Mesh* mesh = meshPtr.get();
            drawMesh(entity, mesh, camera, shadowLightIndex);
        }
    }

        void drawMesh(Entity* entity, Mesh* mesh, const Camera& camera, int shadowLightIndex) {
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
    
    void drawUnlitMesh(Entity* entity, Mesh* mesh, const glm::vec3& color, int intensity) {
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
};

// ============================================================================
// 3D FILE IMPORTER (USING ASSIMP)
// ============================================================================

struct Vertex {
    glm::vec3 position;
    glm::vec4 color;
    glm::vec2 uv;
    glm::vec3 normal;
    glm::vec3 tangent;
    glm::vec3 bitangent;
};

unsigned char* load_greyscale_data(const std::string& path, const aiScene* scene, int& width, int& height) {
    // Check if it is an embedded texture
    if (!path.empty() && path[0] == '*') {
        int texIndex = std::atoi(path.c_str() + 1);
        if (texIndex >= 0 && texIndex < (int)scene->mNumTextures) {
            aiTexture* embeddedTex = scene->mTextures[texIndex];
            
            if (embeddedTex->mHeight == 0) {
                // Compressed texture (PNG, JPG, etc.)
                int channels;
                unsigned char* data = stbi_load_from_memory(
                    (unsigned char*)embeddedTex->pcData,
                    embeddedTex->mWidth,
                    &width, &height, &channels, 1  // Force grayscale
                );
                return data;
            } else {
                // Uncompressed ARGB data - convert to grayscale
                width = embeddedTex->mWidth;
                height = embeddedTex->mHeight;
                unsigned char* data = new unsigned char[width * height];
                
                for (int i = 0; i < width * height; ++i) {
                    // Convert to grayscale using standard luminance formula
                    float r = embeddedTex->pcData[i].r / 255.0f;
                    float g = embeddedTex->pcData[i].g / 255.0f;
                    float b = embeddedTex->pcData[i].b / 255.0f;
                    float gray = 0.299f * r + 0.587f * g + 0.114f * b;
                    data[i] = (unsigned char)(gray * 255.0f);
                }
                return data;
            }
        }
    }
    
    // External texture file
    if (!path.empty()) {
        int channels;
        unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 1);
        return data;
    }
    
    return nullptr;
}

struct ORMResult {
    GLuint textureID;
    bool hasHeightData;
};

ORMResult packORM(const std::string& current_material_name, 
                  const std::string& ao_path, 
                  const std::string& roughness_path, 
                  const std::string& metallic_path, 
                  const std::string& height_path,
                  const std::string& specular_path,
                  const aiScene* scene) {
    int w_ao, h_ao;
    int w_r, h_r;
    int w_m, h_m;
    int w_h, h_h;
    int w_s, h_s;
    
    // Load data for all maps
    unsigned char* ao_data = load_greyscale_data(ao_path, scene, w_ao, h_ao);
    unsigned char* roughness_data = load_greyscale_data(roughness_path, scene, w_r, h_r);
    unsigned char* metallic_data = load_greyscale_data(metallic_path, scene, w_m, h_m);
    unsigned char* height_data = load_greyscale_data(height_path, scene, w_h, h_h);
    unsigned char* specular_data = load_greyscale_data(specular_path, scene, w_s, h_s);
    
    // Must have at least one map to proceed
    if (!ao_data && !roughness_data && !metallic_data && !height_data && !specular_data) {
        return { 0, false };
    }

    int width = 0, height = 0;
    if (ao_data) { width = w_ao; height = h_ao; }
    else if (roughness_data) { width = w_r; height = h_r; }
    else if (metallic_data) { width = w_m; height = h_m; }
    else if (height_data) { width = w_h; height = h_h; }
    else if (specular_data) { width = w_s; height = h_s; }
    
    if (width == 0 || height == 0) return { 0, false };

    // Create packed texture buffer (4 channels: R, G, B, A)
    size_t data_size = (size_t)width * height * 4;
    unsigned char* packed_data = new unsigned char[data_size];

    // Create default channel data
    unsigned char* default_channel_data = new unsigned char[width * height];
    
    // Set up AO channel (default: 1.0 = 255)
    unsigned char* final_ao = ao_data;
    if (!ao_data) {
        memset(default_channel_data, 255, width * height);
        final_ao = default_channel_data;
    }
    
    // Set up Roughness channel
    unsigned char* final_roughness = roughness_data;
    if (!roughness_data && specular_data) {
        // Convert specular to roughness
        final_roughness = new unsigned char[width * height];
        for (int i = 0; i < width * height; ++i) {
            final_roughness[i] = 255 - specular_data[i];
        }
    } else if (!roughness_data) {
        memset(default_channel_data, 255, width * height);
        final_roughness = default_channel_data;
    }
    
    // Set up Metallic channel (default: 0.0 = 0)
    unsigned char* final_metallic = metallic_data;
    if (!metallic_data) {
        memset(default_channel_data, 0, width * height);
        final_metallic = default_channel_data;
    }
    
    bool hasHeightData = (height_data != nullptr);
    unsigned char* final_height = height_data;
    if (!height_data) {
        memset(default_channel_data, 128, width * height);
        final_height = default_channel_data;
    }

    // Pack all channels into RGBA
    for (int i = 0; i < width * height; ++i) {
        packed_data[i * 4] = final_ao[i];            // R = AO
        packed_data[i * 4 + 1] = final_roughness[i]; // G = Roughness
        packed_data[i * 4 + 2] = final_metallic[i];  // B = Metallic
        packed_data[i * 4 + 3] = final_height[i];    // A = Height
    }
    
    GLuint orm_texture_id;
    glGenTextures(1, &orm_texture_id);
    glBindTexture(GL_TEXTURE_2D, orm_texture_id);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, packed_data);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    glBindTexture(GL_TEXTURE_2D, 0);

    // Cleanup
    if (ao_data) stbi_image_free(ao_data);
    if (roughness_data && roughness_data != final_roughness) stbi_image_free(roughness_data);
    if (metallic_data) stbi_image_free(metallic_data);
    if (height_data) stbi_image_free(height_data);
    if (specular_data) stbi_image_free(specular_data);
    if (!roughness_data && specular_data) delete[] final_roughness;
    delete[] packed_data;
    delete[] default_channel_data;
    
    printf("Successfully created packed ORM map for '%s'\n", current_material_name.c_str());
    
    return { orm_texture_id, hasHeightData };
}

Material createMaterialFromAssimp(std::string modelPath, aiMaterial* material, const aiScene* scene) {
    Material mat = createDefaultMaterial();
    aiString path, matName;
    std::string aoPath, roughPath, metalPath, heightPath, specularPath;
    std::string name = "unnamed";

    if (material->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS)
        name = matName.C_Str();

    // Helper lambda to resolve texture path (embedded or external)
    auto resolveTexPath = [&](const aiString& aiPath) -> std::string {
        std::string texPath = aiPath.C_Str();
        if (texPath.empty()) return "";
        
        // If embedded, return as-is
        if (texPath[0] == '*') {
            return texPath;
        }
        
        // For GLTF/GLB files, textures are relative to the model file
        std::filesystem::path modelDir = std::filesystem::path(modelPath).parent_path();
        std::filesystem::path fullTexPath = modelDir / texPath;
        
        // Build the full asset path
        return buildAssetPath("scene_models/" + fullTexPath.string());
    };
    
    // Try to get base color from texture first
    if (material->GetTexture(aiTextureType_BASE_COLOR, 0, &path) == AI_SUCCESS ||
        material->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS) {
        
        std::string texPath = resolveTexPath(path);
        
        // Check if it is an embedded texture
        if (!texPath.empty() && texPath[0] == '*') {
            int texIndex = std::atoi(texPath.c_str() + 1);
            if (texIndex >= 0 && texIndex < (int)scene->mNumTextures) {
                aiTexture* embeddedTex = scene->mTextures[texIndex];
                
                if (embeddedTex->mHeight == 0) {
                    // Compressed texture
                    mat.albedo_map = loadTextureFromMemory((unsigned char*)embeddedTex->pcData, embeddedTex->mWidth);
                } else {
                    // Uncompressed ARGB data
                    mat.albedo_map = loadTextureFromARGB(embeddedTex->pcData, embeddedTex->mWidth, embeddedTex->mHeight);
                }
            }
        } else if (!texPath.empty()) {
            // External texture file
            mat.albedo_map = loadTexture(texPath);
        }
    }
    
    if (!mat.hasAlbedoMap()) {
        aiColor3D color;
        // Try different color keys in order of preference
        if (material->Get(AI_MATKEY_BASE_COLOR, color) == AI_SUCCESS ||
            material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
            mat.base_color = glm::vec3(color.r, color.g, color.b);
        }
    }
    
    if (material->GetTexture(aiTextureType_NORMALS, 0, &path) == AI_SUCCESS ||
        material->GetTexture(aiTextureType_HEIGHT, 0, &path) == AI_SUCCESS) {
        
        std::string texPath = resolveTexPath(path);
        
        if (!texPath.empty() && texPath[0] == '*') {
            int texIndex = std::atoi(texPath.c_str() + 1);
            if (texIndex >= 0 && texIndex < (int)scene->mNumTextures) {
                aiTexture* embeddedTex = scene->mTextures[texIndex];
                
                if (embeddedTex->mHeight == 0) {
                    // Compressed texture
                    mat.normal_map = loadTextureFromMemory((unsigned char*)embeddedTex->pcData, embeddedTex->mWidth);
                } else {
                    // Uncompressed ARGB data
                    mat.normal_map = loadTextureFromARGB(embeddedTex->pcData, embeddedTex->mWidth, embeddedTex->mHeight);
                }
            }
        } else if (!texPath.empty()) {
            // External texture file
            mat.normal_map = loadTexture(texPath);
        }
    }
    
    // Check for metallic texture first
    if (material->GetTexture(aiTextureType_METALNESS, 0, &path) == AI_SUCCESS) {
        metalPath = resolveTexPath(path);
    }
    
    if (metalPath.empty()) {
        float metallicValue;
        if (material->Get(AI_MATKEY_METALLIC_FACTOR, metallicValue) == AI_SUCCESS) {
            mat.metallic = metallicValue;
        }
    }
    
    // Check for roughness texture first
    if (material->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &path) == AI_SUCCESS ||
        material->GetTexture(aiTextureType_SHININESS, 0, &path) == AI_SUCCESS) {
        roughPath = resolveTexPath(path);
    }
    
    // Check for specular map (can be converted to roughness)
    if (roughPath.empty() && material->GetTexture(aiTextureType_SPECULAR, 0, &path) == AI_SUCCESS) {
        specularPath = resolveTexPath(path);
    }
    
    if (roughPath.empty() && specularPath.empty()) {
        float roughnessValue;
        float shininessValue;
        
        // Try roughness factor first
        if (material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughnessValue) == AI_SUCCESS) {
            mat.roughness = roughnessValue;
        }
        // Fall back to shininess (convert to roughness)
        else if (material->Get(AI_MATKEY_SHININESS, shininessValue) == AI_SUCCESS) {
            // Convert shininess to roughness
            // Shininess typically ranges from 0-1000+, normalize and invert
            mat.roughness = 1.0f - glm::clamp(shininessValue / 1000.0f, 0.0f, 1.0f);
        }
    }
    
    // Check for AO texture
    if (material->GetTexture(aiTextureType_AMBIENT_OCCLUSION, 0, &path) == AI_SUCCESS ||
        material->GetTexture(aiTextureType_LIGHTMAP, 0, &path) == AI_SUCCESS ||
        material->GetTexture(aiTextureType_AMBIENT, 0, &path) == AI_SUCCESS) {
        aoPath = resolveTexPath(path);
    }
    
    if (material->GetTexture(aiTextureType_DISPLACEMENT, 0, &path) == AI_SUCCESS ||
        material->GetTexture(aiTextureType_HEIGHT, 0, &path) == AI_SUCCESS) {
        heightPath = resolveTexPath(path);
        
        // Try to get height scale factor
        float heightScale;
        if (material->Get(AI_MATKEY_BUMPSCALING, heightScale) == AI_SUCCESS) {
            mat.height_scale = heightScale;
        }
    }
    
    // Pack ORM map if there are any of the components
    if (!aoPath.empty() || !roughPath.empty() || !metalPath.empty() || !heightPath.empty() || !specularPath.empty()) {
        ORMResult packed = packORM(name, aoPath, roughPath, metalPath, heightPath, specularPath, scene);
        mat.orm_map = packed.textureID;
        if (packed.hasHeightData) mat.height_map = packed.textureID;
    }
    
    // Check for emissive texture first
    if (material->GetTexture(aiTextureType_EMISSIVE, 0, &path) == AI_SUCCESS ||
        material->GetTexture(aiTextureType_EMISSION_COLOR, 0, &path) == AI_SUCCESS) {
        
        std::string texPath = resolveTexPath(path);
        
        if (!texPath.empty() && texPath[0] == '*') {
            int texIndex = std::atoi(texPath.c_str() + 1);
            if (texIndex >= 0 && texIndex < (int)scene->mNumTextures) {
                aiTexture* embeddedTex = scene->mTextures[texIndex];
                
                if (embeddedTex->mHeight == 0) {
                    // Compressed texture
                    mat.emissive_map = loadTextureFromMemory((unsigned char*)embeddedTex->pcData, embeddedTex->mWidth);
                } else {
                    // Uncompressed ARGB data
                    mat.emissive_map= loadTextureFromARGB(embeddedTex->pcData, embeddedTex->mWidth, embeddedTex->mHeight);
                }
            }
        } else if (!texPath.empty()) {
            // External texture file
            mat.emissive_map = loadTexture(texPath);
        }
    }
    
    if (!mat.hasEmissiveMap()) {
        aiColor3D emissiveColor;
        if (material->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor) == AI_SUCCESS) {
            mat.emissive = glm::vec3(emissiveColor.r, emissiveColor.g, emissiveColor.b);
            
            // Also check for emissive intensity/strength multiplier
            float emissiveStrength;
            if (material->Get(AI_MATKEY_EMISSIVE_INTENSITY, emissiveStrength) == AI_SUCCESS) {
                mat.emissive *= emissiveStrength;
            }
        }
    }
    
    mat.name = name;
    return mat;
}

std::vector<std::unique_ptr<Mesh>> loadMesh(const std::string& filepath) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(buildAssetPath("scene_models/" + filepath),
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality |
        aiProcess_OptimizeMeshes |
        aiProcess_CalcTangentSpace |
        aiProcess_PreTransformVertices);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        printf("Assimp error: %s\n", importer.GetErrorString());
        return {};
    }

    std::vector<std::unique_ptr<Mesh>> meshes;
    std::function<void(aiNode*)> processNode = [&](aiNode* node) {
        for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            auto newMesh = std::make_unique<Mesh>();
            
            for (unsigned int v = 0; v < mesh->mNumVertices; ++v) {
                newMesh->vertices_data.push_back(mesh->mVertices[v].x);
                newMesh->vertices_data.push_back(mesh->mVertices[v].y);
                newMesh->vertices_data.push_back(mesh->mVertices[v].z);
                
                // Get vertex colors
                if (mesh->HasVertexColors(0)) {
                    aiColor4D color = mesh->mColors[0][v];
                    newMesh->vertices_data.push_back(color.r);
                    newMesh->vertices_data.push_back(color.g);
                    newMesh->vertices_data.push_back(color.b);
                    newMesh->vertices_data.push_back(color.a);
                } else {
                    // Fallback to white
                    newMesh->vertices_data.push_back(1.0f);
                    newMesh->vertices_data.push_back(1.0f);
                    newMesh->vertices_data.push_back(1.0f);
                    newMesh->vertices_data.push_back(1.0f);
                }
                
                // TexCoords
                if (mesh->HasTextureCoords(0)) {
                    newMesh->vertices_data.push_back(mesh->mTextureCoords[0][v].x);
                    newMesh->vertices_data.push_back(mesh->mTextureCoords[0][v].y);
                } else {
                    newMesh->vertices_data.push_back(0.0f);
                    newMesh->vertices_data.push_back(0.0f);
                }
                
                // Normal
                if (mesh->HasNormals()) {
                    newMesh->vertices_data.push_back(mesh->mNormals[v].x);
                    newMesh->vertices_data.push_back(mesh->mNormals[v].y);
                    newMesh->vertices_data.push_back(mesh->mNormals[v].z);
                } else {
                    newMesh->vertices_data.push_back(0.0f);
                    newMesh->vertices_data.push_back(0.0f);
                    newMesh->vertices_data.push_back(1.0f);
                }
                
                // Tangent
                if (mesh->HasTangentsAndBitangents()) {
                    newMesh->vertices_data.push_back(mesh->mTangents[v].x);
                    newMesh->vertices_data.push_back(mesh->mTangents[v].y);
                    newMesh->vertices_data.push_back(mesh->mTangents[v].z);
                } else {
                    newMesh->vertices_data.push_back(1.0f);
                    newMesh->vertices_data.push_back(0.0f);
                    newMesh->vertices_data.push_back(0.0f);
                }
                
                // Bitangent
                if (mesh->HasTangentsAndBitangents()) {
                    newMesh->vertices_data.push_back(mesh->mBitangents[v].x);
                    newMesh->vertices_data.push_back(mesh->mBitangents[v].y);
                    newMesh->vertices_data.push_back(mesh->mBitangents[v].z);
                } else {
                    newMesh->vertices_data.push_back(0.0f);
                    newMesh->vertices_data.push_back(1.0f);
                    newMesh->vertices_data.push_back(0.0f);
                }
            }
            
            for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
                aiFace face = mesh->mFaces[f];
                for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                    newMesh->indices_data.push_back(face.mIndices[j]);
                }
            }
            
            aiMaterial* assimpMat = scene->mMaterials[mesh->mMaterialIndex];
            newMesh->material = createMaterialFromAssimp(filepath, assimpMat, scene);
            
            newMesh->TRIANGLE_COUNT = mesh->mNumFaces;
            newMesh->INDEX_COUNT = static_cast<unsigned int>(newMesh->indices_data.size());
            
            // Upload to GPU
            glGenVertexArrays(1, &newMesh->VAO);
            glGenBuffers(1, &newMesh->VBO);
            glGenBuffers(1, &newMesh->EBO);
            glBindVertexArray(newMesh->VAO);

            // VBO
            glBindBuffer(GL_ARRAY_BUFFER, newMesh->VBO);
            glBufferData(GL_ARRAY_BUFFER, newMesh->vertices_data.size() * sizeof(float), newMesh->vertices_data.data(), GL_STATIC_DRAW);

            // EBO
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, newMesh->EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, newMesh->indices_data.size() * sizeof(unsigned int), newMesh->indices_data.data(), GL_STATIC_DRAW);

            // Stride = 3 + 4 + 2 + 3 + 3 + 3 = 18 floats
            constexpr GLsizei stride = 18 * sizeof(float);

            // aPos (location = 0)
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)(0));

            // aColor (location = 1)
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));

            // aTexCoords (location = 2)
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(7 * sizeof(float)));

            // aNormal (location = 3)
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(9 * sizeof(float)));

            // aTangent (location = 4)
            glEnableVertexAttribArray(4);
            glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride, (void*)(12 * sizeof(float)));

            // aBitangent (location = 5)
            glEnableVertexAttribArray(5);
            glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, stride, (void*)(15 * sizeof(float)));
            
            glBindVertexArray(0);

            meshes.push_back(std::move(newMesh));
        }

        for (unsigned int i = 0; i < node->mNumChildren; ++i) {
            processNode(node->mChildren[i]);
        }
    };

    processNode(scene->mRootNode);
    return meshes;
}

// ============================================================================
// INITIALIZER
// ============================================================================

int main() {
    
    printf("Starting OpenGL 3D Engine...\n");
    
    // Initialize GLFW
    if (!glfwInit()) {
        printf("Failed to initialize GLFW\n");
        return -1;
    }
    
    // Version number
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    
    // Remove resizability
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    
    // Anti-aliasing
    glfwWindowHint(GLFW_SAMPLES, 4);
    
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "OpenGL 3D Engine by @mu-gua-here", NULL, NULL);
    if (!window) {
        printf("Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    
    // Turn V-Sync off
    glfwSwapInterval(0);
    
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        printf("Failed to initialize GLAD\n");
        return -1;
    }
    
    printf("OpenGL Version: %s\n", glGetString(GL_VERSION));
    
    // Set up ImGui
    IMGUI_CHECKVERSION();
    std::remove("imgui.ini"); // Reset GUI menu state
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    // Set callbacks
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    
    // Performance settings
    glGenQueries(2, shadowQueries);
    glGenQueries(2, mainQueries);
    glGenQueries(2, skyboxQueries);

    // Find executable path
    std::filesystem::path executable_path = getExecutablePath();

    // Create default texture before creating materials
    default_texture_id = createDefaultTexture();
    printf("Created default texture with ID: %u\n", default_texture_id);
    
    // Initialize renderer
    std::unique_ptr<Renderer> renderer;
    try {
        renderer = std::make_unique<Renderer>();
    } catch (const std::exception& e) {
        printf("Renderer error: %s\n", e.what());
        return -1;
    }
    
    // Initialize camera
    global_camera = create_camera(static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT));
    camera_update_vectors(&global_camera);
    
    // Z buffer
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // OpenGL pixel functions
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_MULTISAMPLE);
    
    // ============================================================================
    // LOAD ASSETS
    // ============================================================================
    
    // LOAD SKYBOXES //
    
    // Initialise skybox
    skyboxID = 0;
    Skybox skybox;
    skybox.initShader();
    
    // Cloud skybox
    std::string cloud_skybox_paths[6] = {
        buildAssetPath("skyboxes/Cloud_skybox/cloud_skybox_right.png"),   // GL_TEXTURE_CUBE_MAP_POSITIVE_X
        buildAssetPath("skyboxes/Cloud_skybox/cloud_skybox_left.png"),    // GL_TEXTURE_CUBE_MAP_NEGATIVE_X
        buildAssetPath("skyboxes/Cloud_skybox/cloud_skybox_top.png"),     // GL_TEXTURE_CUBE_MAP_POSITIVE_Y
        buildAssetPath("skyboxes/Cloud_skybox/cloud_skybox_bottom.png"),  // GL_TEXTURE_CUBE_MAP_NEGATIVE_Y
        buildAssetPath("skyboxes/Cloud_skybox/cloud_skybox_front.png"),   // GL_TEXTURE_CUBE_MAP_POSITIVE_Z
        buildAssetPath("skyboxes/Cloud_skybox/cloud_skybox_back.png")     // GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
    };
    
    const char* cloud_skybox[6] = {
        cloud_skybox_paths[0].c_str(),
        cloud_skybox_paths[1].c_str(),
        cloud_skybox_paths[2].c_str(),
        cloud_skybox_paths[3].c_str(),
        cloud_skybox_paths[4].c_str(),
        cloud_skybox_paths[5].c_str()
    };
    
    skybox.bindSkybox(cloud_skybox);
    
    // LOAD OBJ MESHES //
    
    // Note that when importing new assets you only need to add your files to the main assets folders
    // You can find the main asset folders at "OpenGL 3D Engine/scene_models" or "OpenGL 3D Engine/skyboxes"
    
    auto level_mesh = loadMesh("level/level.obj");
    auto tree_mesh = loadMesh("realistic_tree/tree.obj");
    auto instructions_mesh = loadMesh("instructions_panel/quad.obj");
    auto cube_mesh = loadMesh("cube/cube.obj");
    auto sphere_mesh = loadMesh("sphere/sphere.obj");
    auto streetlight_mesh = loadMesh("streetlight/streetlight.obj");
    auto cone_mesh = loadMesh("cone/cone.obj");
    auto statue_mesh = loadMesh("statue/statue_of_myself.obj");
    auto plastic_table = loadMesh("plastic_table/plastic_table.obj");
    auto road_mesh = loadMesh("modular_road/modular_road_pack.obj");
    
    // ============================================================================
    // CREATE SCENE OBJECTS
    // ============================================================================
    
    // CREATE LIGHT SOURCES //
    
    createDirLight("sun", glm::vec3(1, -1, -1), glm::vec3(1, 1, 1), 10);
    /* createPointLight("streetlamp", glm::vec3(-7.05, 3.5, 0.05), glm::vec3(1, 1, 1), 250,
                    {}, glm::vec3(0.1, 0.1, 0.1), std::vector<int>{CULL_BACK}); */
    createSpotlight("torchlight", glm::vec3(0, 10, 0), glm::vec3(1, 1, 1), 50,
                    glm::vec3(0, -1, 0), 7.5f, 17.5f,
                    {}, glm::vec3(0.1, 0.1, 0.1), std::vector<int>{CULL_BACK});
    
    // Initialise shadow map
    initShadowMap();

    // CREATE ENTITIES //
    
    createEntity("level", std::move(level_mesh), glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), glm::vec3(10, 10, 10), std::vector<int> {CULL_NONE});
    createEntity("tree", std::move(tree_mesh), glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), std::vector<int>{CULL_BACK, CULL_NONE});
    createEntity("instructions", std::move(instructions_mesh), glm::vec3(0, 2, 4), glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), std::vector<int> {CULL_NONE});
    createEntity("cube", std::move(cube_mesh), glm::vec3(5, 3, 0), glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), std::vector<int> {CULL_BACK});
    createEntity("sphere", std::move(sphere_mesh), glm::vec3(0, 2, -5), glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), std::vector<int> {CULL_BACK});
    createEntity("streetlight", std::move(streetlight_mesh), glm::vec3(-7, 0.02, 0), glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), std::vector<int>{CULL_BACK});
    createEntity("cone", std::move(cone_mesh), glm::vec3(0, 10, 0), glm::vec3(0, 0, 0), glm::vec3(0.1, 0.1, 0.1), std::vector<int>{CULL_BACK});
    createEntity("statue", std::move(statue_mesh), glm::vec3(-5, 1.9, -4), glm::vec3(0, 0, 0), glm::vec3(0.1, 0.1, 0.1), std::vector<int>{CULL_BACK});
    createEntity("plastic_table", std::move(plastic_table), glm::vec3(-5, 0, -4), glm::vec3(0, 0, 0), glm::vec3(0.5, 0.5, 0.5), std::vector<int>{CULL_BACK});
    createEntity("road", std::move(road_mesh), glm::vec3(-50, 0, 0), glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), std::vector<int>{CULL_BACK});

    printf("Total triangles: %d\n", total_triangles);
    printf("Active entities: %zu\n", entity_manager.size());
    
    // ============================================================================
    // MAIN LOOP
    // ============================================================================
    
    while (!glfwWindowShouldClose(window)) {
        double frameStart = glfwGetTime();
        updateFPS(window);
        
        if (!paused) {
            
            // ============================================================================
            // PLAYER TICK
            // ============================================================================
            
            float yaw_rad = global_camera.yaw * M_PI / 180.0f;
            float sin_yaw = sinf(yaw_rad);
            float cos_yaw = cosf(yaw_rad);
            glm::vec3 cam_offset = glm::vec3(0.0f);
            float actual_cam_speed = tick_speed * cam_speed_multiplier;
            
            // Sprint
            if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
                actual_cam_speed *= 2;
            }
            
            // Horizontal movement
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
                cam_offset = cam_offset + glm::vec3(cos_yaw, 0, sin_yaw);
            }
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
                cam_offset = cam_offset + glm::vec3(-cos_yaw, 0, -sin_yaw);
            }
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
                cam_offset = cam_offset + glm::vec3(sin_yaw, 0, -cos_yaw);
            }
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                cam_offset = cam_offset + glm::vec3(-sin_yaw, 0, cos_yaw);
            }

            if (glm::length(cam_offset) > 0.0f) {
                cam_offset = glm::normalize(cam_offset);
            }

            global_camera_vel.x += cam_offset.x * actual_cam_speed;
            global_camera_vel.z += cam_offset.z * actual_cam_speed;
            
            // Vertical movement
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
                global_camera_vel.y += actual_cam_speed;
            }
            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
                global_camera_vel.y -= actual_cam_speed;
            }

            global_camera_vel *= friction;
            
            global_camera.position = global_camera.position + global_camera_vel;
            
            // Update camera matrices
            view = camera_get_view_matrix(&global_camera);
            projection = camera_get_projection(&global_camera);
            
            // ============================================================================
            // UPDATE SCENE OBJECTS
            // ============================================================================
            
            // UPDATE LIGHTS
            
            updateLight("torchlight", glm::vec3(global_camera.position + global_camera.front + glm::vec3(-sin_yaw * 0.3f, 0, cos_yaw * 0.3f)),
                        VEC3_NO_CHANGE, -1, glm::vec3(global_camera.front), glm::vec3(0, 0, 0));
            
            // UPDATE OBJECTS
            
            entity_manager.updateEntity("cube", VEC3_NO_CHANGE, glm::vec3(update_count * 0.1f, update_count * 0.1f, update_count * 0.1f), VEC3_NO_CHANGE);
            entity_manager.updateEntity("sphere", glm::vec3(NAN, 2.5f + sinf(update_count * 0.01f), NAN), glm::vec3(update_count, 0, 0), VEC3_NO_CHANGE);
            entity_manager.updateEntity("cone", glm::vec3(global_camera.position + global_camera.front + glm::vec3(-sin_yaw * 0.3f, 0, cos_yaw * 0.3f)), convertVecToEuler(global_camera.front, glm::vec3(90, 0, 0)), VEC3_NO_CHANGE);
            entity_manager.updateEntity("statue", VEC3_NO_CHANGE, glm::vec3(NAN, update_count, NAN), VEC3_NO_CHANGE);

            update_count += tick_speed;
        }
        
        // Clear background
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Render shadow map
        glBeginQuery(GL_TIME_ELAPSED, shadowQueries[queryIndex]);
        int shadowLightIndex = -1;
        for (size_t i = 0; i < lights.size(); i++) {
            renderer->renderShadowPass(entity_manager, lights[i]);
            shadowLightIndex = static_cast<int>(i);
            break; // Only render shadows for the first shadow-casting light
        }
        glEndQuery(GL_TIME_ELAPSED);
                
        // Render skybox before other objects
        glBeginQuery(GL_TIME_ELAPSED, skyboxQueries[queryIndex]);
        skybox.render(&global_camera);
        glEndQuery(GL_TIME_ELAPSED);
        
        // Set PBR shader uniforms
        glBeginQuery(GL_TIME_ELAPSED, mainQueries[queryIndex]);
        renderer->setGlobalUniforms(global_camera);

        // Render all entities
        for (size_t i = 0; i < entity_manager.size(); i++) {
            Entity* entity = entity_manager.getEntityAt(i);
            if (entity && entity->active) {
                // Check if this entity is a light representation
                bool is_light = false;
                for (const auto& light : lights) {
                    if (entity->name == light.entity_name) {
                        is_light = true;
                        // Render as unlit emissive object
                        for (const auto& meshPtr : entity->meshes) {
                            Mesh* mesh = meshPtr.get();
                            if (mesh && mesh->isValid()) {
                                renderer->drawUnlitMesh(entity, mesh, light.color, light.intensity);
                            }
                        }
                        break;
                    }
                }
                
                // Render normally if not a light
                if (!is_light) {
                    renderer->drawEntity(entity, global_camera, shadowLightIndex);
                }
            }
        }
        glEndQuery(GL_TIME_ELAPSED);

        queryIndex = 1 - queryIndex;

        glfwPollEvents();

        prevIndex = 1 - queryIndex;        

        // ============================================================================
        // UI & WINDOW CONTROL
        // ============================================================================
        
        // UPDATE IMGUI GUI
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(10, 10));
        ImGui::Begin("Engine");
        ImGui::Text("FPS: %.1f", fps);
        ImGui::Text("Triangles: %u", total_triangles);
        ImGui::Text("GPU Frame Time:");
        ImGui::Text("Shadows: %.3f ms", shadowTime);
        ImGui::Text("Skybox: %.3f ms", skyboxTime);
        ImGui::Text("Main: %.3f ms", mainTime);
        ImGui::Text("Total GPU: %.3f ms", shadowTime + skyboxTime + mainTime);
        if (ImGui::Button("V-Sync ON")) glfwSwapInterval(1);
        if (ImGui::Button("V-Sync OFF")) glfwSwapInterval(0);
        ImGui::End();

        // Handle mouse cursor pointerlock/normal modes
        if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_NORMAL && !ImGui::GetIO().WantCaptureMouse) {
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) {
                firstMouse = true;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // Enable pointerlock
                paused = false;
            }
        } else {
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); // Disable pointerlock
                paused = true;
            }
        }
        
        // Handle fullscreen entry/exit
        static bool fullscreen_toggle = false;
        if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS && glfwGetKey(window, GLFW_KEY_LEFT_ALT) && !fullscreen_toggle) {
            fullscreen_toggle = true;
            
            GLFWmonitor *monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode *vm = glfwGetVideoMode(monitor);
            if (!fullscreen) {
                // Enter fullscreen
                WINDOW_WIDTH = vm->width;
                WINDOW_HEIGHT = vm->height;
                glfwSetWindowMonitor(window, monitor, 0, 0, vm->width, vm->height, vm->refreshRate);
                fullscreen = true;
            } else {
                // Leave fullscreen
                WINDOW_WIDTH = 800;
                WINDOW_HEIGHT = 600;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); // Free cursor
                glfwSetWindowMonitor(window, nullptr, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
                fullscreen = false;
                
                // Center the window
                int w, h;
                glfwGetWindowSize(window, &w, &h);
                GLFWmonitor *m = glfwGetPrimaryMonitor();
                int mx, my, mw, mh;
                glfwGetMonitorWorkarea(m, &mx, &my, &mw, &mh);
                glfwSetWindowPos(window, mx + (mw - w) / 2, my + (mh - h) / 2);
            }
        }
        if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_RELEASE && glfwGetKey(window, GLFW_KEY_LEFT_ALT)) fullscreen_toggle = false;

        // Pause menu (temporary)
        if (paused) {
            ImGui::SetNextWindowPos(ImVec2(WINDOW_WIDTH / 2 - 75, WINDOW_HEIGHT / 2 - 25));
            ImGui::Begin("PAUSED");
            ImGui::Text("CLICK AGAIN TO UNPAUSE");
            ImGui::End();
        }

        // END GUI FRAME & RENDER
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glFlush();
    }
    
    // ============================================================================
    // CLEANUP
    // ============================================================================
    
    printf("Cleaning up...\n");
    skybox.cleanup();
    
    if (default_texture_id != 0) {
        glDeleteTextures(1, &default_texture_id);
        default_texture_id = 0;
    }

    // Cleanup GPU timers
    glDeleteQueries(2, shadowQueries);
    glDeleteQueries(2, mainQueries);
    glDeleteQueries(2, skyboxQueries);
    
    cleanupShadowMap();
    
    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

// ============================================================================
// INPUT CALLBACKS
// ============================================================================

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    static double lastX = 400.0;
    static double lastY = 300.0;
    
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    
    if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED) return;
    
    double xoffset = xpos - lastX;
    double yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    xoffset *= mouse_sensitivity;
    yoffset *= mouse_sensitivity;
    
    global_camera.yaw += xoffset;
    global_camera.pitch += yoffset;

    if (global_camera.pitch > 89.0f) global_camera.pitch = 89.0f;
    if (global_camera.pitch < -89.0f) global_camera.pitch = -89.0f;
    
    camera_update_vectors(&global_camera);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    WINDOW_WIDTH = width;
    WINDOW_HEIGHT = height;
    (void)window;
    glViewport(0, 0, width, height);
    if (height > 0) {
        global_camera.aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
    }
}
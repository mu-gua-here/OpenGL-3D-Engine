//
//  main.cpp
//  OpenGL 3D Engine
//
//  Created by Ray Hsiao Muguang on 2025/9/15.
//

/*
 
 NOTES
 
 IMPLEMENTATION LIST
 1. Add collision detection
 2. Physics (maybe)
 3. Add entity instancing & batching
 4. Optimisations
    a. Batching
    b. Instancing
    c. Frustum culling
    d. LOD
 
 BUGS / IMPROVEMENTS LIST
 1. Entity deletion optimization (memory leaks are present in current code)
 2. Fix goofy-ahh AO and roughness broken texture
 3. Shadow mapping not working for directional lights
 4. Fix broken POM :cry:
 
 */

// OpenGL-related
#include <GLFW/glfw3.h>
#include "glad/glad.h"

// ImGui
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// Texture loader
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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

// Filesystem access
#include <filesystem>
#ifdef __APPLE__
    #include <mach-o/dyld.h>
#endif

// GLM library
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

// Function prototypes
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// Window
int WINDOW_WIDTH = 800;
int WINDOW_HEIGHT = 600;

// Runtime
bool firstMouse = true;
double fps;
bool paused = false;
float update_count = 0;
float frame_time = 1.0f;
float speed_multiplier = 1.0f;
glm::mat4 view;
glm::mat4 projection;
bool fullscreen = false;

// Scene stats
unsigned int total_triangles = 0;
unsigned int entity_count = 0;

// Shadow mapping
GLuint shadowMapFBO = 0;
GLuint shadowMapTexture = 0;
const unsigned int SHADOW_WIDTH = 4096;
const unsigned int SHADOW_HEIGHT = 4096;
glm::mat4 lightSpaceMatrix;

// Game settings
#define MAX_LIGHTS 8
float mouse_sensitivity = 0.1f;

// Skybox
unsigned int skyboxID;

GLuint default_texture_id = 0;

const glm::vec3 VEC3_NO_CHANGE = glm::vec3(NAN, NAN, NAN);
const float SCALAR_NO_CHANGE = NAN;

void updateFPS(GLFWwindow* window) {
    static double lastTime = glfwGetTime();
    static unsigned int frameCount = 0;
    static double fpsTimer = 0.0;
    
    double currentTime = glfwGetTime();
    frame_time = currentTime - lastTime;
    lastTime = currentTime;
    speed_multiplier = frame_time / 0.0083f;
    
    frameCount++;
    fpsTimer += frame_time;
    
    if (fpsTimer >= 1.0) {
        fps = frameCount / fpsTimer;
        char title[256];
        snprintf(title, sizeof(title), "OpenGL 3D Engine by @mu-gua-here");
        glfwSetWindowTitle(window, title);
        frameCount = 0;
        fpsTimer = 0.0;
    }
}

// ============================================================================
// ASSET PATH BUILDING
// ============================================================================

std::string getProjectRoot() {
    std::filesystem::path exe_path;
    
    #ifdef __APPLE__
        char path[1024];
        uint32_t size = sizeof(path);
        if (_NSGetExecutablePath(path, &size) == 0) {
            exe_path = std::filesystem::canonical(path).parent_path();
        } else {
            exe_path = std::filesystem::current_path();
        }
    #else
        exe_path = std::filesystem::current_path();
    #endif
    
    std::filesystem::path project_root = exe_path;
    
    // Search upward for OBJ_Models directory (will find source directory)
    while (!std::filesystem::exists(project_root / "OBJ_Models") &&
           project_root != project_root.parent_path()) {
        project_root = project_root.parent_path();
    }
    
    if (!std::filesystem::exists(project_root / "OBJ_Models")) {
        printf("ERROR: Could not find OBJ_Models directory!\n");
        printf("Searched up from: %s\n", exe_path.string().c_str());
    }
    
    return project_root.string();
}

std::string buildAssetPath(const std::string& relative_path) {
    static std::string project_root;
    if (project_root.empty()) {
        project_root = getProjectRoot();
    }
    return project_root + "/" + relative_path;
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
    
    // PBR properties
    glm::vec3 base_color{1.0f, 1.0f, 1.0f};  // Used if no albedo map
    float metallic = 1.0f;                    // Used if no metallic map
    float roughness = 0.5f;                   // Used if no roughness map
    glm::vec3 emissive{0.0f, 0.0f, 0.0f};    // Used if no emissive map
    
    // Texture IDs - 0 means "not present"
    GLuint albedo_map = 0;      // Base color texture
    GLuint normal_map = 0;      // Normal/bump map
    GLuint orm_map = 0;         // AO, roughnesss and metallic maps combined
    GLuint emissive_map = 0;    // Self-illumination
    
    // For metallic-roughness combined textures (common in glTF)
    GLuint metallic_roughness_map = 0;  // R = unused, G = roughness, B = metallic
    
    // Legacy texture settings
    GLuint texture_id = 0;
    Color diffuse_color;
    
    float height_scale = 0.0f;
    
    Material() = default;
    Material(const std::string& name) : name(name) {}
    
    // Check if a specific map is present
    bool hasAlbedoMap() const { return albedo_map != 0; }
    bool hasNormalMap() const { return normal_map != 0; }
    bool hasORMMap() const { return orm_map != 0; }
    bool hasEmissiveMap() const { return emissive_map != 0; }
    bool hasMetallicRoughnessMap() const { return metallic_roughness_map != 0; }
    bool hasHeightMap() const { return orm_map != 0; }
};

Material createDefaultMaterial() {
    Material default_mat;
    default_mat.name = "default";
    default_mat.texture_id = default_texture_id;
    default_mat.albedo_map = default_texture_id;  // PBR fallback
    default_mat.diffuse_color = {1.0f, 1.0f, 1.0f, 1.0f};
    default_mat.base_color = {1.0f, 1.0f, 1.0f};
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
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
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
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
                 SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    
    // Attach depth texture to framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMapTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        printf("Error: Shadow map framebuffer is not complete!\n");
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
    
    bool updateEntity(std::string name, const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scale) {
        for (size_t i = 0; i < entities.size(); i++) {
            if (entities[i].active && entities[i].name == name) {
                
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
        }
        return false;
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
    entity.rotation.x = rotation.x;
    entity.rotation.y = rotation.y;
    entity.rotation.z = rotation.z;
    entity.scale = scale;
    entity.active = 1;
    
    // Apply cull modes for individual submeshes
    for (size_t i = 0; i < meshes.size(); i++) {
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
    cam.far_plane = 100.0f;
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
    glm::vec3 initial_direction;
    glm::vec3 euler_rotation;
    float inner_cutoff_cos;
    float outer_cutoff_cos;
    int type;
    
    std::string entity_name;
} Light;

std::vector<Light> lights;

// Helper function for directional lights
glm::mat4 computeDirLightSpaceMatrix (const glm::vec3& direction, const Camera& cam, float halfSize, float zNear, float zFar) {
    glm::vec3 L = glm::normalize(direction); // light shines in –L
    glm::vec3 up = std::abs(L.y) > 0.999f ? glm::vec3(0,0,1)
                                          : glm::vec3(0,1,0);
    glm::mat4 V = glm::lookAt(cam.position - L * zFar * 0.5f,
                              cam.position, up);
    glm::mat4 P = glm::ortho(-halfSize, halfSize,
                             -halfSize, halfSize,
                             zNear, zFar);
    return P * V;
}

void createDirLight(std::string name, glm::vec3 direction, glm::vec3 color, int intensity) {
    Light light;
    light.position = direction * 1000.0f;
    light.color = color;
    light.intensity = intensity * 1000;
    
    // Set spotlight-only settings to invalid
    light.direction = glm::vec3(0.0f, -1.0f, 0.0f);
    light.inner_cutoff_cos = -1.0f;
    light.outer_cutoff_cos = -1.0f;
    light.type = DIR_LIGHT;
    
    light.entity_name = name;
    lights.push_back(light);
}

void createSpotlight(std::string name, glm::vec3 position, glm::vec3 color, int intensity,
                     glm::vec3 initial_direction, float inner_angle_deg, float outer_angle_deg, glm::vec3 rotation,
                     std::vector<std::unique_ptr<Mesh>>&& light_mesh, glm::vec3 scale, std::vector<int> cull_mode) {
    Light light;
    light.position = position;
    light.color = color;
    light.intensity = intensity;
    
    // Store the initial/local direction and rotation
    light.initial_direction = glm::normalize(initial_direction);
    light.euler_rotation = rotation;
    
    light.direction = glm::normalize(initial_direction);
    
    light.inner_cutoff_cos = glm::cos(glm::radians(inner_angle_deg));
    light.outer_cutoff_cos = glm::cos(glm::radians(outer_angle_deg));
    light.type = SPOT_LIGHT;

    light.entity_name = name;
    lights.push_back(light);
    
    // The light's entity also needs to be created with the rotation
    createEntity(light.entity_name, std::move(light_mesh), position, rotation, scale, cull_mode);
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
    lights.push_back(light);
    createEntity(light.entity_name, std::move(light_mesh), position, light.direction, scale, cull_mode);
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
in mat3 TBN;          // Rows: T, B, N (world to tangent)

out vec4 FragColor;

// Samplers
uniform sampler2D albedoMap;
uniform sampler2D normalMap;
uniform sampler2D ormMap; // R = AO  G = Roughness  B = Metallic  A = Height
uniform sampler2D emissiveMap;
uniform sampler2D shadowMap;

// Materials
uniform vec3 baseColor;
uniform float metallic;
uniform float roughness;
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
uniform int  shadowLightIndex;

uniform vec3 lightPositions[MAX_LIGHTS];
uniform vec3 lightColors[MAX_LIGHTS];
uniform float lightIntensities[MAX_LIGHTS];
uniform vec3 lightDirections[MAX_LIGHTS];
uniform float lightInnerCutoffs[MAX_LIGHTS];
uniform float lightOuterCutoffs[MAX_LIGHTS];
uniform float lightTypes[MAX_LIGHTS];

const float PI = 3.14159265359;

// PARALLAX OCCLUSION MAPPING (POM)
vec2 parallaxMapping(vec2 uv, vec3 Vts) {
    const float minLayers = 8.0;
    const float maxLayers = 32.0;
    float numLayers = mix(maxLayers, minLayers, abs(Vts.z));   // more layers when grazing
    float layerDepth = 1.0 / numLayers;
    vec2 deltaUV = Vts.xy * heightScale / numLayers;

    float currDepth = 0.0;
    vec2  currUV = uv;
    float height = texture(ormMap, currUV).a;

    // Ray-march until we go *below* the surface
    for (float i = 0.0; i < numLayers; ++i) {
        currUV -= deltaUV;
        height = texture(ormMap, currUV).a;
        currDepth += layerDepth;
        if (currDepth >= height) break;
    }

    // Refine with a second linear step
    vec2 prevUV = currUV + deltaUV;
    float prevH = texture(ormMap, prevUV).a;
    float nextH = height;
    float w = (currDepth - currDepth + layerDepth - prevH) / (nextH - prevH + 1e-4);
    currUV = mix(prevUV, currUV, w);

    return currUV;
}

// SHADOW MAPPING

float calcShadow(vec4 fragLight, vec3 N, vec3 L) {
    if (shadowLightIndex < 0) return 0.0;

    // Project shadow
    vec3 proj = fragLight.xyz / fragLight.w;
    proj = proj * 0.5 + 0.5;

    if (proj.z > 1.0) return 0.0;

    // Apply bias to account for self-shadowing acne
    float bias = max(0.2 * tan(acos(dot(N, L))), 0.02);
    vec3 shadowPos = vec3(fragLight) + L * bias; // Push vertex along normal
    proj.z = (shadowPos.z / fragLight.w) * 0.5 + 0.5 - bias; // Apply to depth

    float shadow = 0.0;
    vec2 texel = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            shadow += texture(shadowMap, proj.xy + vec2(x,y) * texel).r < proj.z ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    // Soft border fade
    vec2 border = min(proj.xy, 1.0 - proj.xy);
    float fade  = smoothstep(0.0, 0.1, min(border.x, border.y));
    return mix(0.0, shadow, fade);
}

// BRDF PBR
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float D_GGX(vec3 N, vec3 H, float a2) {
    float NdotH = max(dot(N, H), 0.0);
    float denom = NdotH*NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

float G_Schlick(float NdotV, float k) {
    return NdotV / (NdotV * (1.0 - k) + k);
}

float G_Smith(vec3 N, vec3 V, vec3 L, float rough) {
    float k = (rough + 1.0) * (rough + 1.0) / 8.0;
    return G_Schlick(max(dot(N, L), 0.0), k) * G_Schlick(max(dot(N, V), 0.0), k);
}

// MAIN LOOP
void main() {
    // Tangent-space POM
    vec3 Vworld = normalize(viewPos - FragPos);
    vec3 Vts = normalize(transpose(TBN) * Vworld); // View dir in tangent space
    vec2 uv;
    if (!hasHeightMap) {
        uv = TexCoord;
    } else {
        uv = parallaxMapping(TexCoord, Vts);
    }

    // Sample texture
    vec4 albedoS = hasAlbedoMap ? texture(albedoMap, uv) : vec4(baseColor, 1.0);
    if (albedoS.a < 0.5) discard;

    vec3 albedo = albedoS.rgb;
    vec4 orm = hasORMMap ? texture(ormMap, uv) : vec4(1.0, roughness, metallic, 0.5);
    float ao = orm.r;
    float rough = orm.g;
    float metal = orm.b;

    vec3 emissiveCol = hasEmissiveMap ? texture(emissiveMap, uv).rgb : emissive;

    vec3 N = normalize(Normal);
    if (hasNormalMap) {
        vec3 nt = texture(normalMap, uv).rgb * 2.0 - 1.0;
        N = normalize(TBN * nt);
    }
    
    // Flip normal if facing away
    if (!gl_FrontFacing) N = -N;

    vec3 V = normalize(Vworld);
    vec3 F0 = mix(vec3(0.04), albedo, metal);
    vec3 Lo = vec3(0.0);

    for (int i = 0; i < lightCount && i < MAX_LIGHTS; ++i) {
        // Calculate vector based on whether light is directional or not
        vec3 L = (lightTypes[i] < 0.5) ? normalize(-lightPositions[i]) : normalize(lightPositions[i] - FragPos);
        vec3 H = normalize(V + L);
        float dist = length(lightPositions[i] - FragPos);
        float att = 1.0 / (dist * dist);
        vec3 radiance = lightColors[i] * lightIntensities[i] * att;

        // Spotlight attenuation
        if (lightTypes[i] > 0.5) {
            float theta = dot(-L, lightDirections[i]);
            float eps = lightInnerCutoffs[i] - lightOuterCutoffs[i];
            float spot = clamp((theta - lightOuterCutoffs[i]) / eps, 0.0, 1.0);
            radiance *= spot;
        }

        // Cook-torrance
        float NDF = D_GGX(N, H, rough * rough);
        float G = G_Smith(N, V, L, rough);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metal;

        vec3 numerator = NDF * G * F;
        float denom = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denom;

        float shadow = (i == shadowLightIndex) ? calcShadow(FragPosLightSpace, N, L) : 0.0;
        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL * (1.0 - shadow);
    }

    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color   = ambient + Lo + emissiveCol;

    // Tonemap & gamma correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));

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
        glm::mat4 view = glm::mat4(glm::mat3(camera_get_view_matrix(camera)));
        glm::mat4 projection = camera_get_projection(camera);
        
        skybox_shader->setMat4("view", view);
        skybox_shader->setMat4("projection", projection);
        
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
        for (size_t i = 0; i < cubemap_texture.size(); i++) {
            if (cubemap_texture[i] != 0) {
                glDeleteTextures(1, &cubemap_texture[i]);
                cubemap_texture[i] = 0;
            }
        }
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
        
        if (light.type == SPOT_LIGHT) {
            // Move along the light's actual direction vector
            glm::vec3 lightTarget = light.position + light.direction;
            glm::vec3 finalDir = light.direction;
            
            // Choose up vector dynamically based on the calculated light direction
            glm::vec3 up = glm::abs(finalDir.y) > 0.99f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
            
            glm::mat4 lightView = glm::lookAt(light.position, lightTarget, up);
            float outerAngle = glm::degrees(glm::acos(light.outer_cutoff_cos));
            lightProjection = glm::perspective(glm::radians(outerAngle * 2.0f), 1.0f, 0.5f, 100.0f);
            
            lightSpaceMatrix = lightProjection * lightView;
            
        } else if (light.type == POINT_LIGHT) {
            // Omni-directional point light
            glm::vec3 lightTarget = glm::vec3(0.0f, 0.0f, 0.0f);
            glm::vec3 finalDir = glm::normalize(lightTarget - light.position);

            glm::vec3 up = glm::abs(finalDir.y) > 0.99f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
            
            lightProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.5f, 100.0f);
            glm::mat4 lightView = glm::lookAt(light.position, lightTarget, up);
            
            lightSpaceMatrix = lightProjection * lightView;
            
        } else if (light.type == DIR_LIGHT) {
            // Directional light
            lightSpaceMatrix = computeDirLightSpaceMatrix(light.position, global_camera, 40.0f, 0.1f, 200.0f);
        }
        
        shadow_shader->use();
        shadow_shader->setMat4("lightSpaceMatrix", lightSpaceMatrix);
        
        // Render all entities
        for (size_t i = 0; i < entity_manager.size(); i++) {
            Entity* entity = entity_manager.getEntityAt(i);
            if (entity && entity->active) {
                bool is_light_entity = false;
                for (const auto& light : lights) {
                    if (entity->name == light.entity_name) {
                        is_light_entity = true;
                        break;
                    }
                }
                if (is_light_entity) {
                    continue;  // Skip rendering this entity in shadow pass
                }
                for (const auto& mesh : entity->meshes) {
                    if (mesh && mesh->isValid()) {
                        // Apply mesh-specific culling
                        if (mesh->cull_mode == CULL_NONE) {
                            glDisable(GL_CULL_FACE);  // Disable culling for thin objects
                        } else {
                            glEnable(GL_CULL_FACE);
                            glCullFace(GL_FRONT);     // Keep front-face culling for solid objects
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
    
    void setLightingUniforms() {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> colors;
        std::vector<float> intensities;
        std::vector<glm::vec3> directions;
        std::vector<float> inner_cutoffs;
        std::vector<float> outer_cutoffs;
        std::vector<float> types;

        for (const auto& light : lights) {
            glm::mat4 rotationMatrix(1.0f);
            rotationMatrix = glm::rotate(rotationMatrix, glm::radians(light.euler_rotation.y), glm::vec3(0.0f, 1.0f, 0.0f)); // Yaw
            rotationMatrix = glm::rotate(rotationMatrix, glm::radians(light.euler_rotation.x), glm::vec3(1.0f, 0.0f, 0.0f)); // Pitch
            rotationMatrix = glm::rotate(rotationMatrix, glm::radians(light.euler_rotation.z), glm::vec3(0.0f, 0.0f, 1.0f)); // Roll
                        
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
        
        pbr_shader->use();
        
        // Camera uniforms
        pbr_shader->setMat4("view", view);
        pbr_shader->setMat4("projection", projection);
        pbr_shader->setMat4("lightSpaceMatrix", lightSpaceMatrix);
        pbr_shader->setVec3("viewPos", camera.position);
        
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
        
        // Set material properties (fallback values)
        pbr_shader->setVec3("baseColor", mesh->material.base_color);
        pbr_shader->setFloat("metallic", mesh->material.metallic);
        pbr_shader->setFloat("roughness", mesh->material.roughness);
        pbr_shader->setVec3("emissive", mesh->material.emissive);
        
        // Bind textures and set flags
        int texture_unit = 0;
        
        // Albedo map
        glActiveTexture(GL_TEXTURE0 + texture_unit);
        GLuint albedo_to_use = mesh->material.hasAlbedoMap() ?
        mesh->material.albedo_map : default_texture_id;
        glBindTexture(GL_TEXTURE_2D, albedo_to_use);
        pbr_shader->setInt("albedoMap", texture_unit);
        pbr_shader->setInt("hasAlbedoMap", mesh->material.hasAlbedoMap());
        texture_unit++;
        
        // Normal map
        glActiveTexture(GL_TEXTURE0 + texture_unit);
        GLuint normal_to_use = mesh->material.hasNormalMap() ?
        mesh->material.normal_map : default_texture_id;
        glBindTexture(GL_TEXTURE_2D, normal_to_use);
        pbr_shader->setInt("normalMap", texture_unit);
        pbr_shader->setInt("hasNormalMap", mesh->material.hasNormalMap());
        texture_unit++;
        
        // ORM map
        glActiveTexture(GL_TEXTURE0 + texture_unit);
        if (mesh->material.hasORMMap()) {
            glBindTexture(GL_TEXTURE_2D, mesh->material.orm_map);
        } else {
            glBindTexture(GL_TEXTURE_2D, default_texture_id);
        }
        pbr_shader->setInt("ormMap", texture_unit);
        pbr_shader->setFloat("heightScale", mesh->material.height_scale);
        pbr_shader->setInt("hasHeightMap", mesh->material.hasORMMap());
        
        // Emissive map
        glActiveTexture(GL_TEXTURE0 + texture_unit);
        GLuint emissive_to_use = mesh->material.hasEmissiveMap() ?
        mesh->material.emissive_map : default_texture_id;
        glBindTexture(GL_TEXTURE_2D, emissive_to_use);
        pbr_shader->setInt("emissiveMap", texture_unit);
        pbr_shader->setInt("hasEmissiveMap", mesh->material.hasEmissiveMap());
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
        
        // Disable culling for light spheres to ensure they're always visible
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
// OBJ AND MTL LOADERS
// ============================================================================

void loadMTL(const std::string& mtl_path, std::unordered_map<std::string, Material>& materials);

// Updated Vertex structure with tangent and bitangent
struct Vertex {
    glm::vec3 position;
    glm::vec4 color;
    glm::vec2 texcoord;
    glm::vec3 normal;
    glm::vec3 tangent;
    glm::vec3 bitangent;

    bool operator==(const Vertex& other) const {
        return position == other.position &&
               texcoord == other.texcoord &&
               normal == other.normal &&
               color == other.color &&
               tangent == other.tangent &&
               bitangent == other.bitangent;
    }
};

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(const Vertex& v) const {
            return ((hash<float>()(v.position.x) ^ hash<float>()(v.position.y) << 1) >> 1)
                 ^ (hash<float>()(v.position.z) << 1)
                 ^ (hash<float>()(v.texcoord.x) << 2)
                 ^ (hash<float>()(v.texcoord.y) << 3)
                 ^ (hash<float>()(v.normal.x) << 4)
                 ^ (hash<float>()(v.normal.y) << 5)
                 ^ (hash<float>()(v.normal.z) << 6)
                 ^ (hash<float>()(v.tangent.x) << 7)
                 ^ (hash<float>()(v.tangent.y) << 8)
                 ^ (hash<float>()(v.tangent.z) << 9);
        }
    };
}

// Function to calculate tangents and bitangents for a triangle
void calculateTangentBitangent(
    const glm::vec3& pos1, const glm::vec3& pos2, const glm::vec3& pos3,
    const glm::vec2& uv1, const glm::vec2& uv2, const glm::vec2& uv3,
    glm::vec3& tangent, glm::vec3& bitangent)
{
    glm::vec3 edge1 = pos2 - pos1;
    glm::vec3 edge2 = pos3 - pos1;
    glm::vec2 deltaUV1 = uv2 - uv1;
    glm::vec2 deltaUV2 = uv3 - uv1;
    
    float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
    
    tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
    tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
    tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
    
    bitangent.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
    bitangent.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
    bitangent.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
}

std::vector<std::unique_ptr<Mesh>> loadOBJWithMTL(const std::string& obj_path) {
    std::ifstream file(obj_path);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << obj_path << std::endl;
        return {};
    }
    
    std::unordered_map<std::string, Material> materials;
    std::unordered_map<std::string, std::vector<Vertex>> unique_vertices;
    std::unordered_map<std::string, std::vector<unsigned int>> indices_by_material;
    std::unordered_map<std::string, std::unordered_map<Vertex, unsigned int>> vertex_to_index;
    std::string current_material_name;
    
    // First pass to load MTL file
    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string token;
        ss >> token;

        if (token == "mtllib") {
            std::string mtl_filename;
            ss >> mtl_filename;
            std::string obj_dir = obj_path.substr(0, obj_path.find_last_of('/'));
            std::string mtl_path = obj_dir + "/" + mtl_filename;
            
            loadMTL(mtl_path, materials);
            break;
        }
    }
    file.clear();
    file.seekg(0, std::ios::beg);

    if (materials.empty()) {
        materials["default"] = createDefaultMaterial();
        current_material_name = "default";
    }

    std::vector<glm::vec3> temp_vertices;
    std::vector<glm::vec2> temp_texcoords;
    std::vector<glm::vec3> temp_normals;
    
    // Temporary storage for faces (we need to calculate tangents after all faces are loaded)
    struct FaceVertex {
        int v_idx, vt_idx, vn_idx;
    };
    struct Face {
        std::vector<FaceVertex> vertices;
        std::string material;
    };
    std::vector<Face> faces;

    // Second pass to load vertex data and faces
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string token;
        ss >> token;

        if (token == "v") {
            glm::vec3 vertex;
            ss >> vertex.x >> vertex.y >> vertex.z;
            temp_vertices.push_back(vertex);
        } else if (token == "vt") {
            glm::vec2 texcoord;
            ss >> texcoord.x >> texcoord.y;
            temp_texcoords.push_back(texcoord);
        } else if (token == "vn") {
            glm::vec3 normal;
            ss >> normal.x >> normal.y >> normal.z;
            temp_normals.push_back(normal);
        } else if (token == "usemtl") {
            ss >> current_material_name;
        } else if (token == "f") {
            Face face;
            face.material = current_material_name;
            
            std::string vertex_str;
            while (ss >> vertex_str) {
                std::stringstream v_ss(vertex_str);
                std::string v, vt, vn;
                std::getline(v_ss, v, '/');
                std::getline(v_ss, vt, '/');
                std::getline(v_ss, vn, '/');

                if (v.empty()) continue;
                
                FaceVertex fv;
                fv.v_idx = std::stoi(v) - 1;
                fv.vt_idx = vt.empty() ? -1 : std::stoi(vt) - 1;
                fv.vn_idx = vn.empty() ? -1 : std::stoi(vn) - 1;
                
                face.vertices.push_back(fv);
            }
            
            if (face.vertices.size() >= 3) {
                faces.push_back(face);
            }
        }
    }
    
    // Now process faces and calculate tangents
    for (const auto& face : faces) {
        const std::string& mat_name = face.material;
        
        // Fan triangulation
        for (size_t i = 1; i < face.vertices.size() - 1; i++) {
            std::vector<Vertex> triangle_verts(3);
            
            // Build the three vertices of this triangle
            std::vector<int> tri_indices = {0, (int)i, (int)i + 1};
            
            for (int j = 0; j < 3; j++) {
                const FaceVertex& fv = face.vertices[tri_indices[j]];
                
                if (fv.v_idx < 0 || fv.v_idx >= (int)temp_vertices.size()) continue;
                
                triangle_verts[j].position = temp_vertices[fv.v_idx];
                triangle_verts[j].texcoord = (fv.vt_idx >= 0 && fv.vt_idx < (int)temp_texcoords.size())
                    ? temp_texcoords[fv.vt_idx] : glm::vec2(0.0f, 0.0f);
                triangle_verts[j].normal = (fv.vn_idx >= 0 && fv.vn_idx < (int)temp_normals.size())
                    ? temp_normals[fv.vn_idx] : glm::vec3(0.0f, 1.0f, 0.0f);
                triangle_verts[j].color = materials[mat_name].diffuse_color.toVec4();
            }
            
            // Calculate tangent and bitangent for this triangle
            glm::vec3 tangent, bitangent;
            calculateTangentBitangent(
                triangle_verts[0].position, triangle_verts[1].position, triangle_verts[2].position,
                triangle_verts[0].texcoord, triangle_verts[1].texcoord, triangle_verts[2].texcoord,
                tangent, bitangent
            );
            
            // Apply the same tangent/bitangent to all 3 vertices
            for (int j = 0; j < 3; j++) {
                triangle_verts[j].tangent = tangent;
                triangle_verts[j].bitangent = bitangent;
                
                // Add to mesh data
                auto& v_map = vertex_to_index[mat_name];
                auto& verts = unique_vertices[mat_name];
                auto& inds = indices_by_material[mat_name];

                if (v_map.count(triangle_verts[j]) == 0) {
                    v_map[triangle_verts[j]] = static_cast<unsigned int>(verts.size());
                    verts.push_back(triangle_verts[j]);
                }

                inds.push_back(v_map[triangle_verts[j]]);
            }
        }
    }

    // Create meshes
    std::vector<std::unique_ptr<Mesh>> meshes;
    for (const auto& pair : unique_vertices) {
        const std::string& mat_name = pair.first;
        const std::vector<Vertex>& vertices = pair.second;
        const std::vector<unsigned int>& indices = indices_by_material[mat_name];

        Mesh* mesh = new Mesh();
        mesh->TRIANGLE_COUNT = static_cast<unsigned int>(indices.size() / 3);
        mesh->INDEX_COUNT = static_cast<unsigned int>(indices.size());
        mesh->material = materials.at(mat_name);

        glGenVertexArrays(1, &mesh->VAO);
        glGenBuffers(1, &mesh->VBO);
        glGenBuffers(1, &mesh->EBO);

        glBindVertexArray(mesh->VAO);

        glBindBuffer(GL_ARRAY_BUFFER, mesh->VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        GLsizei stride = sizeof(Vertex);
        // Position
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, position));
        glEnableVertexAttribArray(0);
        // Color
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, color));
        glEnableVertexAttribArray(1);
        // Texcoord
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, texcoord));
        glEnableVertexAttribArray(2);
        // Normal
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(3);
        // Tangent
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, tangent));
        glEnableVertexAttribArray(4);
        // Bitangent
        glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, bitangent));
        glEnableVertexAttribArray(5);

        glBindVertexArray(0);

        meshes.emplace_back(mesh);
    }
    
    return meshes;
}

// Helper function to discard and ignore optional flags in MTL files
void parse_texture_map_line(std::stringstream& ss, std::string& texture_filename) {
    std::string token;
    
    while (ss >> token) {
        // Check for flags that take one numerical argument
        if (token == "-bm" || token == "-bl") {
            // Consume and discard the next token (the numerical value).
            std::string arg;
            if (!(ss >> arg)) {
                // Handle case where flag is present but value is missing
                printf("Warning: Missing argument for flag '%s'\n", token.c_str());
            }
            continue;
        }
        
        // Check for flags that take three numerical arguments
        else if (token == "-o" || token == "-s" || token == "-t") {
            // Consume and discard the next THREE tokens (u, v, w)
            std::string arg1, arg2, arg3;
            ss >> arg1 >> arg2 >> arg3;
            continue;
        }
        
        // Check for flags that take no arguments
        else if (token == "-clamp" || token == "-mm") {
            continue;
        }
        
        // If the token is not a recognized flag, assume it is the filename itself
        else if (token.length() > 0 && token[0] != '-') {
            
            texture_filename = token;
            
            std::string remainder;
            std::getline(ss, remainder);
            if (!remainder.empty()) {
                // Append the remaining part of the line if any, usually after trimming leading spaces
                remainder.erase(0, remainder.find_first_not_of(" \t\n\r\f\v"));
                if (!remainder.empty()) {
                    texture_filename += " " + remainder;
                }
            }
            break;
        }
    }
}

unsigned char* load_grayscale_data(const std::string& path, int& width, int& height) {
    int channels; // Will be the actual channels in the file
    // Force the output to be 1 channel (GL_RED)
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 1);
    if (!data && !path.empty()) {
        std::cerr << "Warning: Failed to load single-channel map: " << path << std::endl;
    }
    return data;
}

GLuint packAndLoadORMMap(const std::string& current_material_name, const std::string& ao_path, const std::string& roughness_path, const std::string& metallic_path, const std::string& height_path) {
    int w_ao, h_ao;
    int w_r, h_r;
    int w_m, h_m;
    int w_h, h_h;
    
    // Load data for all three maps
    unsigned char* ao_data = load_grayscale_data(ao_path, w_ao, h_ao);
    unsigned char* roughness_data = load_grayscale_data(roughness_path, w_r, h_r);
    unsigned char* metallic_data = load_grayscale_data(metallic_path, w_m, h_m);
    unsigned char* height_data = load_grayscale_data(height_path, w_h, h_h);
    
    // Must have at least one map to proceed
    if (!ao_data && !roughness_data && !metallic_data && !height_data) {
        return 0;
    }

    int width = 0, height = 0;
    if (ao_data) { width = w_ao; height = h_ao; }
    else if (roughness_data) { width = w_r; height = h_r; }
    else if (metallic_data) { width = w_m; height = h_m; }
    else if (height_data) { width = w_h; height = h_h; }
    
    if (width == 0 || height == 0) return 0;

    // Create packed texture buffer (4 channels: R, G, B, A)
    size_t data_size = (size_t)width * height * 4;
    unsigned char* packed_data = new unsigned char[data_size];

    // Create a fallback 1-channel buffer (all white, value 255) for missing maps
    // AO = 1.0, Roughness = 1.0, Metallic = 0.0
    unsigned char* default_channel_data = new unsigned char[width * height];
    
    // Use loaded data or default to white/black
    unsigned char* final_ao = ao_data ? ao_data : default_channel_data;
    unsigned char* final_roughness = roughness_data ? roughness_data : default_channel_data;
    
    // Metallic is typically 0.0 (black) for the default dielectric base,
    // so we must handle its fallback specially if you want a non-metal look
    unsigned char* final_metallic = metallic_data ? metallic_data : default_channel_data;
    if (!metallic_data) memset(final_metallic, 0, width * height); // Set metallic fallback to 0 (black)
    
    // Height map fallback
    unsigned char* final_height = height_data ? height_data : default_channel_data;
    if (!height_data) memset(final_height, 128, width * height);

    for (int i = 0; i < width * height; ++i) {
        // R channel gets AO (Occlusion)
        packed_data[i * 4 + 0] = final_ao[i];
        // G channel gets Roughness
        packed_data[i * 4 + 1] = final_roughness[i];
        // B channel gets Metallic
        packed_data[i * 4 + 2] = final_metallic[i];
        // A channel gets Height
        packed_data[i * 4 + 3] = final_height[i];
    }
    
    GLuint orm_texture_id;
    glGenTextures(1, &orm_texture_id);
    glBindTexture(GL_TEXTURE_2D, orm_texture_id);
    
    // Standard texture parameters (reusing from your loadTexture)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, packed_data);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    glBindTexture(GL_TEXTURE_2D, 0);

    if (ao_data) stbi_image_free(ao_data);
    if (roughness_data) stbi_image_free(roughness_data);
    if (metallic_data) stbi_image_free(metallic_data);
    if (height_data) stbi_image_free(height_data);
    delete[] packed_data;
    delete[] default_channel_data;
    
    printf("Successfully created and loaded packed ORM (RGBA) map for material: %s\n", current_material_name.c_str());
    return orm_texture_id;
}

void loadMTL(const std::string& mtl_path, std::unordered_map<std::string, Material>& materials) {
    std::ifstream file(mtl_path);
    if (!file.is_open()) {
        std::cerr << "Warning: Cannot open MTL file " << mtl_path << std::endl;
        return;
    }
    
    std::string current_material_name;
    std::string line;
    
    // Temporary path storage for packing
    std::string current_roughness_path;
    std::string current_metallic_path;
    std::string current_ao_path;
    std::string current_height_path;
    
    // Helper to get full path
    auto get_full_path = [&](const std::string& filename) -> std::string {
        std::string mtl_dir = mtl_path.substr(0, mtl_path.find_last_of('/'));
        return mtl_dir + "/" + filename;
    };
    
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string token;
        ss >> token;
        
        if (token == "newmtl") {
            if (!current_material_name.empty() && materials.count(current_material_name)) {
                // Only pack if at least one map path was found for the previous material
                if (!current_roughness_path.empty() || !current_metallic_path.empty() || !current_ao_path.empty() || !current_height_path.empty()) {
                    materials[current_material_name].orm_map = packAndLoadORMMap(current_material_name, current_ao_path, current_roughness_path, current_metallic_path, current_height_path);
                }
            }
            
            // Start new material
            ss >> current_material_name;
            materials[current_material_name] = createDefaultMaterial();
            materials[current_material_name].name = current_material_name;
            
            // Reset temporary paths for the new material
            current_roughness_path.clear();
            current_metallic_path.clear();
            current_ao_path.clear();
            current_height_path.clear();
            
            // PBR scalar maps
        } else if (token == "Pr" && materials.count(current_material_name)) {
            // Roughness value (0.0 to 1.0)
            ss >> materials[current_material_name].roughness;
            
        } else if (token == "Pm" && materials.count(current_material_name)) {
            // Metallic value (0.0 to 1.0)
            ss >> materials[current_material_name].metallic;
            
        } else if (token == "Ke" && materials.count(current_material_name)) {
            // Emissive color
            ss >> materials[current_material_name].emissive.x
            >> materials[current_material_name].emissive.y
            >> materials[current_material_name].emissive.z;
            
        // Legacy texture maps
        } else if (token == "map_Kd" && materials.count(current_material_name)) {
            
            std::string texture_filename;
            ss >> texture_filename;
            std::string mtl_dir = mtl_path.substr(0, mtl_path.find_last_of('/'));
            std::string full_texture_path = mtl_dir + "/" + texture_filename;
            
            GLuint tex_id = loadTexture(full_texture_path);
            materials[current_material_name].texture_id = tex_id;
            materials[current_material_name].albedo_map = tex_id;  // Use as albedo
            
            if (tex_id == 0) {
                std::cerr << "Warning: Failed to load texture " << full_texture_path
                << " for material " << current_material_name << std::endl;
            }
            
        // PBR texture maps
        } else if ((token == "map_Ka" || token == "map_albedo" || token == "map_base_color")
                   && materials.count(current_material_name)) {
            // Albedo/base color map (diffuse without lighting info)
            std::string texture_filename;
            parse_texture_map_line(ss, texture_filename);
            std::string mtl_dir = mtl_path.substr(0, mtl_path.find_last_of('/'));
            std::string full_texture_path = mtl_dir + "/" + texture_filename;
            
            materials[current_material_name].albedo_map = loadTexture(full_texture_path);
            printf("Loaded albedo map for '%s': %s\n", current_material_name.c_str(), texture_filename.c_str());
            
        } else if ((token == "map_Pr" || token == "map_roughness") && materials.count(current_material_name)) {
            // Roughness map
            std::string texture_filename;
            parse_texture_map_line(ss, texture_filename);
            current_roughness_path = get_full_path(texture_filename);
            printf("Found roughness map path for '%s': %s\n", current_material_name.c_str(), current_roughness_path.c_str());
            
        } else if ((token == "map_Pm" || token == "map_metallic") && materials.count(current_material_name)) {
            // Metallic map
            std::string texture_filename;
            parse_texture_map_line(ss, texture_filename);
            current_metallic_path = get_full_path(texture_filename);
            printf("Found metallic map path for '%s': %s\n", current_material_name.c_str(), current_metallic_path.c_str());
            
        } else if (token == "map_ao" && materials.count(current_material_name)) {
            // Ambient occlusion map
            std::string texture_filename;
            parse_texture_map_line(ss, texture_filename);
            current_ao_path = get_full_path(texture_filename);
            printf("Found AO map path for '%s': %s\n", current_material_name.c_str(), current_ao_path.c_str());
            
        } else if ((token == "disp" || token == "map_disp") && materials.count(current_material_name)) {
            // Height map
            std::string texture_filename;
            parse_texture_map_line(ss, texture_filename);
            current_height_path = get_full_path(texture_filename);
            printf("Found height map path for '%s': %s\n", current_material_name.c_str(), current_height_path.c_str());
            
        } else if ((token == "norm" || token == "map_Bump" || token == "bump") && materials.count(current_material_name)) {
            // Normal map
            std::string texture_filename;
            parse_texture_map_line(ss, texture_filename);
            std::string mtl_dir = mtl_path.substr(0, mtl_path.find_last_of('/'));
            std::string full_texture_path = mtl_dir + "/" + texture_filename;
            
            materials[current_material_name].normal_map = loadTexture(full_texture_path);
            printf("Loaded normal map for '%s': %s\n", current_material_name.c_str(), texture_filename.c_str());
            
        } else if ((token == "map_Ke" || token == "map_emissive")
                   && materials.count(current_material_name)) {
            // Emissive map
            std::string texture_filename;
            parse_texture_map_line(ss, texture_filename);
            std::string mtl_dir = mtl_path.substr(0, mtl_path.find_last_of('/'));
            std::string full_texture_path = mtl_dir + "/" + texture_filename;
            
            materials[current_material_name].emissive_map = loadTexture(full_texture_path);
            printf("Loaded emissive map for '%s': %s\n", current_material_name.c_str(), texture_filename.c_str());
            
        } else if (token == "map_metallic_roughness" && materials.count(current_material_name)) {
            // Combined metallic-roughness map (glTF style: R = unused, G = roughness, B = metallic)
            std::string texture_filename;
            parse_texture_map_line(ss, texture_filename);
            std::string mtl_dir = mtl_path.substr(0, mtl_path.find_last_of('/'));
            std::string full_texture_path = mtl_dir + "/" + texture_filename;
            
            materials[current_material_name].metallic_roughness_map = loadTexture(full_texture_path);
            printf("Loaded metallic-roughness map for '%s': %s\n", current_material_name.c_str(), texture_filename.c_str());
        }
    }
    
    if (!current_material_name.empty() && materials.count(current_material_name)) {
        if (!current_roughness_path.empty() || !current_metallic_path.empty() || !current_ao_path.empty() || !current_height_path.empty()) {
            materials[current_material_name].orm_map = packAndLoadORMMap(current_material_name, current_ao_path, current_roughness_path, current_metallic_path, current_height_path);
        }
    }
}

std::vector<std::unique_ptr<Mesh>> loadOBJMesh(const std::string& obj_path) {
    const std::string& full_obj_path = buildAssetPath("OBJ_Models/" + obj_path);
    if (!std::filesystem::exists(full_obj_path)) {
        std::cerr << "ERROR::OBJ_LOAD::File does not exist: " << full_obj_path << std::endl;
        
        // File is missing or permission denied
        std::cerr << "Check your file path or OS file permissions." << std::endl;
        
        // Return empty mesh
        return {};
    }
    
    std::vector<std::unique_ptr<Mesh>> meshes = loadOBJWithMTL(full_obj_path);

    if (meshes.empty()) {
        std::cerr << "Failed to load OBJ file: " << full_obj_path << std::endl;
        return {};
    } else {
        std::cerr << "Successfully loaded OBJ file: " << full_obj_path << std::endl;
    }

    entity_count++;
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
    
    // Anti-aliasing
    glfwWindowHint(GLFW_SAMPLES, 4);
    
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "OpenGL 3D Engine", NULL, NULL);
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
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    // Set callbacks
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    
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
    
    // Initialise shadow map
    initShadowMap();
    
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
        buildAssetPath("Skyboxes/Cloud_skybox/cloud_skybox_right.png"),   // GL_TEXTURE_CUBE_MAP_POSITIVE_X
        buildAssetPath("Skyboxes/Cloud_skybox/cloud_skybox_left.png"),    // GL_TEXTURE_CUBE_MAP_NEGATIVE_X
        buildAssetPath("Skyboxes/Cloud_skybox/cloud_skybox_top.png"),     // GL_TEXTURE_CUBE_MAP_POSITIVE_Y
        buildAssetPath("Skyboxes/Cloud_skybox/cloud_skybox_bottom.png"),  // GL_TEXTURE_CUBE_MAP_NEGATIVE_Y
        buildAssetPath("Skyboxes/Cloud_skybox/cloud_skybox_front.png"),   // GL_TEXTURE_CUBE_MAP_POSITIVE_Z
        buildAssetPath("Skyboxes/Cloud_skybox/cloud_skybox_back.png")     // GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
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
    // You can find the main asset folders at "OpenGL 3D Engine/OpenGL 3D Engine/OBJ_Models" or "OpenGL 3D Engine/OpenGL 3D Engine/Skyboxes"
    
    auto level_mesh = loadOBJMesh("Level/level.obj");
    auto tree_mesh = loadOBJMesh("Realistic_tree/tree.obj");
    auto instructions_mesh = loadOBJMesh("Instructions_Panel/quad.obj");
    auto cube_mesh = loadOBJMesh("Cube/cube.obj");
    auto sphere_mesh = loadOBJMesh("Sphere/sphere.obj");
    auto streetlight_mesh = loadOBJMesh("Streetlight/streetlight.obj");
    auto cone_mesh = loadOBJMesh("Cone/cone.obj");
    
    // ============================================================================
    // CREATE SCENE OBJECTS
    // ============================================================================
    
    // CREATE LIGHT SOURCES //
    
    createDirLight("sun", glm::vec3(0, 1, 0), glm::vec3(1, 1, 1), 2500);
    createPointLight("streetlamp", glm::vec3(-7.05, 3.5, 0.05), glm::vec3(1, 1, 1), 500,
                    {}, glm::vec3(0.1, 0.1, 0.1), std::vector<int>{CULL_BACK});
    createSpotlight("torchlight", glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), 250,
                    glm::vec3(1, 0, 0), 12.5f, 17.5f, glm::vec3(0, 0, 0),
                    {}, glm::vec3(0.1, 0.1, 0.1), std::vector<int>{CULL_BACK});
    
    // CREATE ENTITIES //
    
    createEntity("level", std::move(level_mesh), glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), glm::vec3(10, 10, 10), std::vector<int> {CULL_NONE});
    createEntity("tree", std::move(tree_mesh), glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), std::vector<int>{CULL_NONE, CULL_BACK});
    createEntity("instructions", std::move(instructions_mesh), glm::vec3(0, 2, 4), glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), std::vector<int> {CULL_NONE});
    createEntity("cube", std::move(cube_mesh), glm::vec3(5, 3, 0), glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), std::vector<int> {CULL_BACK});
    createEntity("sphere", std::move(sphere_mesh), glm::vec3(0, 2, -5), glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), std::vector<int> {CULL_BACK});
    createEntity("streetlight", std::move(streetlight_mesh), glm::vec3(-7, 0.05, 0), glm::vec3(1, 1, 1), glm::vec3(1, 1, 1), std::vector<int>{CULL_BACK});
    
    printf("Total triangles: %d\n", total_triangles);
    printf("Active entities: %zu\n", entity_manager.size());
    
    // ============================================================================
    // MAIN LOOP
    // ============================================================================
    
    while (!glfwWindowShouldClose(window)) {
        updateFPS(window);
        
        if (!paused) {
            
            // ============================================================================
            // PLAYER TICK
            // ============================================================================
            
            float yaw_rad = global_camera.yaw * M_PI / 180.0f;
            float sin_yaw = sinf(yaw_rad);
            float cos_yaw = cosf(yaw_rad);
            glm::vec3 cam_offset = glm::vec3(0.0f);
            float camera_speed = speed_multiplier * 0.05f;
            
            // Sprint
            if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
                camera_speed *= 2;
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
            
            // Vertical movement
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
                global_camera.position.y += camera_speed;
            }
            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
                global_camera.position.y -= camera_speed;
            }
            
            if (glm::length(cam_offset) > 0.0f) {
                cam_offset = glm::normalize(cam_offset);
            }
            global_camera.position = global_camera.position + cam_offset * camera_speed;
            
            // Update camera matrices
            view = camera_get_view_matrix(&global_camera);
            projection = camera_get_projection(&global_camera);
            
            // ============================================================================
            // UPDATE SCENE OBJECTS
            // ============================================================================
            
            // UPDATE LIGHTS
            
            lights[2].position = glm::vec3(global_camera.position);
            lights[2].direction = glm::vec3(global_camera.front);
            
            // UPDATE OBJECTS
            
            // entity_manager.updateEntity("cube", VEC3_NO_CHANGE, glm::vec3(update_count, update_count * 0.5f, SCALAR_NO_CHANGE), VEC3_NO_CHANGE);
            entity_manager.updateEntity("sphere", glm::vec3(SCALAR_NO_CHANGE, 2.5 + sinf(update_count * 0.01f), SCALAR_NO_CHANGE), glm::vec3(update_count, 0, 0), VEC3_NO_CHANGE);
            
            update_count += speed_multiplier;
        }
        
        // Clear background
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        renderer->setLightingUniforms();
        
        // Render shadow map
        int shadowLightIndex = -1;
        for (size_t i = 0; i < lights.size(); i++) {
            renderer->renderShadowPass(entity_manager, lights[i]);
            shadowLightIndex = static_cast<int>(i);
            break; // Only render shadows for the first shadow-casting light
        }
                
        // Render skybox before other objects
        skybox.render(&global_camera);
        
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
        
        glfwPollEvents();
        
        // ============================================================================
        // UI & WINDOW CONTROL
        // ============================================================================
        
        // Render and update ImGui GUI
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        ImGui::Begin("Engine");
        ImGui::Text("FPS: %.1f", fps);
        ImGui::Text("Triangles: %u", total_triangles);
        if (ImGui::Button("V-Sync ON"))  glfwSwapInterval(1);
        if (ImGui::Button("V-Sync OFF")) glfwSwapInterval(0);
        ImGui::End();
        
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window);
        
        // Handle mouse cursor pointerlock/normal modes
        if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_NORMAL) {
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) {
                if (!ImGui::GetIO().WantCaptureMouse) {
                    firstMouse = true;
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // Enable pointerlock
                }
            }
        } else {
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); // Disable pointerlock
            }
        }
        
        // Handle fullscreen entry/exit
        static bool f_pressed  = false;
        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS && !f_pressed) {
            f_pressed = true;
            
            GLFWmonitor *monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode *vm = glfwGetVideoMode(monitor);
            if (!fullscreen) {
                // Enter fullscreen
                WINDOW_WIDTH = vm->width;
                WINDOW_HEIGHT = vm->height;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); // free cursor
                glfwSetWindowMonitor(window, monitor, 0, 0, vm->width, vm->height, vm->refreshRate);
                fullscreen = true;
            } else {
                // Leave fullscreen
                WINDOW_WIDTH = 800;
                WINDOW_HEIGHT = 600;
                glfwSetWindowMonitor(window, nullptr, 0, 0, 800, 600, 0);
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
        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_RELEASE) f_pressed = false;
    }
    
    // ============================================================================
    // CLEANUP
    // ============================================================================
    
    printf("Cleaning up...\n");
    skybox.cleanup();
    
    if (default_texture_id != 0) {
        glDeleteTextures(1, &default_texture_id);
    }
    
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

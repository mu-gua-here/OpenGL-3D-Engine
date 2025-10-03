//
//  main.cpp
//  OpenGL 3D Engine
//
//  Created by Ray Hsiao Muguang on 2025/9/15.
//

/*
 
 IMPLEMENTATION LIST
 1. Add collision detection
 2. Add PBR maps
 3. Physics (maybe)
 4. Add entity instancing & batching
 5. Optimisations
    a. Batching
    b. Instancing
    c. Frustum culling
    d. LOD
 
 BUGS / IMPROVEMENTS LIST
 1. Entity deletion optimization (memory leaks are present in code) -> use
 
 */

// OpenGL internal
#include <GLFW/glfw3.h>
#include "glad/glad.h"

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
#include <unordered_map>
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
#define MAX_LIGHTS 16
float mouse_sensitivity = 0.1f;

GLuint default_texture_id = 0;

std::unordered_set<class Mesh*> global_mesh_registry;
const glm::vec3 VEC3_NO_CHANGE = glm::vec3(NAN, NAN, NAN);

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
        snprintf(title, sizeof(title),
            "OpenGL 3D Engine - FPS: %.1f | Frame time: %.3f ms",
            fps, frame_time * 1000.0);
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

// Material class with PBR texture support
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
    GLuint metallic_map = 0;    // Metallic values
    GLuint roughness_map = 0;   // Roughness values
    GLuint ao_map = 0;          // Ambient occlusion
    GLuint emissive_map = 0;    // Self-illumination
    
    // For metallic-roughness combined textures (common in glTF)
    GLuint metallic_roughness_map = 0;  // R = unused, G = roughness, B = metallic
    
    // Legacy texture (for backward compatibility with your current code)
    GLuint texture_id = 0;
    Color diffuse_color;
        
    Material() = default;
    Material(const std::string& name) : name(name) {}
    
    // Check if a specific map is present
    bool hasAlbedoMap() const { return albedo_map != 0; }
    bool hasNormalMap() const { return normal_map != 0; }
    bool hasMetallicMap() const { return metallic_map != 0; }
    bool hasRoughnessMap() const { return roughness_map != 0; }
    bool hasAOMap() const { return ao_map != 0; }
    bool hasEmissiveMap() const { return emissive_map != 0; }
    bool hasMetallicRoughnessMap() const { return metallic_roughness_map != 0; }
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
    
    size_t vertex_count;
    size_t index_count;
    unsigned int TRIANGLE_COUNT;
    unsigned int INDEX_COUNT;
    GLuint VAO, VBO, EBO;
    GLuint texture_id;
    Material material;
    int cull_mode;
    bool is_cleaned_up;
    
    Mesh() : vertex_count(0), index_count(0), TRIANGLE_COUNT(0), VAO(0), VBO(0), EBO(0), texture_id(0), cull_mode(CULL_NONE), is_cleaned_up(false) {
        material = createDefaultMaterial();
        global_mesh_registry.insert(this);
    }
    
    ~Mesh() {
        cleanup();
        global_mesh_registry.erase(this);
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
        
        vertex_count = index_count = TRIANGLE_COUNT = INDEX_COUNT = 0;
        is_cleaned_up = true;
    }
    
    void setVertices(const std::vector<float>& vertices) {
        vertices_data = vertices;
        vertex_count = vertices.size();
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
// ENTITY SYSTEM
// ============================================================================

typedef struct {
    std::string name;
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
    std::vector<Mesh*> meshes;
    int active;
} Entity;

class EntityManager {
private:
    std::vector<Entity> entities;
    std::vector<bool> active_flags;

public:
    size_t addEntity(const Entity& entity) {
        entities.push_back(entity);
        active_flags.push_back(true);
        return entities.size() - 1;
    }
    
    bool updateEntity(std::string name, const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scale) {
        for (size_t i = 0; i < entities.size(); i++) {
            if (active_flags[i] && entities[i].name == name) {
                if (!isnan(pos.x)) entities[i].position = pos;
                if (!isnan(rot.x)) {
                    entities[i].rotation.x = rot.x;
                    entities[i].rotation.y = rot.y;
                    entities[i].rotation.z = rot.z;
                }
                if (!isnan(scale.x)) entities[i].scale = scale;
                return true;
            }
        }
        return false;
    }
    
    size_t size() const { return entities.size(); }
    
    Entity* getEntityAt(size_t index) {
        if (index < entities.size() && active_flags[index]) {
            return &entities[index];
        }
        return nullptr;
    }
    
    void removeEntity(size_t index) {
        if (index >= entities.size() || !active_flags[index]) return;
        
        Entity& entity = entities[index];
        
        // Subtract from triangle count
        for (Mesh* mesh : entity.meshes) {
            if (mesh) {
                total_triangles -= mesh->TRIANGLE_COUNT;
            }
        }
        
        // Clear the mesh references (but don't delete the meshes themselves)
        entity.meshes.clear();
        
        // Mark as inactive
        active_flags[index] = false;
    }

    void removeEntity(const std::string& name) {
        for (size_t i = 0; i < entities.size(); i++) {
            if (active_flags[i] && entities[i].name == name) {
                removeEntity(i);
                return;
            }
        }
    }
};

EntityManager entity_manager;

void createEntity(std::string name, std::vector<Mesh*> meshes, glm::vec3 pos, glm::vec3 rotation, glm::vec3 scale, std::vector<int> cull_modes) {
    Entity entity;
    entity.name = name;
    entity.position = pos;
    entity.rotation.x = rotation.x;
    entity.rotation.y = rotation.y;
    entity.rotation.z = rotation.z;
    entity.scale = scale;
    entity.meshes = meshes;
    entity.active = 1;
    entity_manager.addEntity(entity);
    
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
    
    unsigned int mesh_triangles = 0;
    for (Mesh* mesh : meshes) {
        if (mesh) mesh_triangles += mesh->TRIANGLE_COUNT;
    }
    total_triangles += mesh_triangles;
    
    printf("Created entity '%s' with %zu submesh(es) (triangles: %u)\n",
           name.c_str(), meshes.size(), mesh_triangles);
}

// ============================================================================
// LIGHTING SYSTEM
// ============================================================================

typedef struct {
    glm::vec3 position;
    glm::vec3 color;
    float intensity;
    std::string entity_name;
} Light;

std::vector<Light> lights;

void createPointLight(std::string name, glm::vec3 position, glm::vec3 color, float intensity, std::vector<Mesh*> light_mesh, glm::vec3 scale, glm::vec3 mesh_offset, std::vector<int> cull_mode) {
    Light light;
    light.position = position;
    light.color = color;
    light.intensity = intensity;
    light.entity_name = name;
    lights.push_back(light);
    createEntity(light.entity_name, light_mesh, position + mesh_offset, glm::vec3(0, 0, 0), scale, cull_mode);
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
    cam.far_plane = 500.0f;
    return cam;
}

void camera_update_vectors(Camera* cam) {
    cam->front.x = cos(glm::radians(cam->yaw)) * cos(glm::radians(cam->pitch));
    cam->front.y = sin(glm::radians(cam->pitch));
    cam->front.z = sin(glm::radians(cam->yaw)) * cos(glm::radians(cam->pitch));
    cam->front = glm::normalize(cam->front);

    cam->right = glm::normalize(cross(cam->front, glm::vec3(0, 1, 0)));
    cam->up = glm::normalize(cross(cam->right, cam->front));
}

glm::mat4 camera_get_projection(Camera* cam) {
    return glm::perspective(cam->fov, cam->aspect_ratio, cam->near_plane, cam->far_plane);
}

glm::mat4 camera_get_view_matrix(Camera* cam) {
    return lookAt(cam->position, cam->position + cam->front, cam->up);
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
out vec3 Normal;
out vec4 FragPosLightSpace;
out mat3 TBN;  // Tangent-Bitangent-Normal matrix for normal mapping

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;
uniform mat4 lightSpaceMatrix;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = normalMatrix * aNormal;
    gl_Position = projection * view * vec4(FragPos, 1.0);
    TexCoord = aTexCoords;
    vertexColor = aColor;
    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);
    
    // Construct TBN matrix for normal mapping
    vec3 T = normalize(vec3(model * vec4(aTangent, 0.0)));
    vec3 B = normalize(vec3(model * vec4(aBitangent, 0.0)));
    vec3 N = normalize(vec3(model * vec4(aNormal, 0.0)));
    TBN = mat3(T, B, N);
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

// Textures
uniform sampler2D albedoMap;
uniform sampler2D normalMap;
uniform sampler2D metallicMap;
uniform sampler2D roughnessMap;
uniform sampler2D aoMap;
uniform sampler2D emissiveMap;
uniform sampler2D shadowMap;

// Material properties (used if maps are not present)
uniform vec3 baseColor;
uniform float metallic;
uniform float roughness;
uniform vec3 emissive;

// Texture presence flags
uniform bool hasAlbedoMap;
uniform bool hasNormalMap;
uniform bool hasMetallicMap;
uniform bool hasRoughnessMap;
uniform bool hasAOMap;
uniform bool hasEmissiveMap;

// Lighting
#define MAX_LIGHTS 16
uniform vec3 lightPositions[MAX_LIGHTS];
uniform vec3 lightColors[MAX_LIGHTS];
uniform float lightIntensities[MAX_LIGHTS];
uniform int lightCount;
uniform vec3 viewPos;
uniform int shadowLightIndex;

const float PI = 3.14159265359;

// Normal Distribution Function (GGX/Trowbridge-Reitz)
// Determines how much the surface's microfacets are aligned with the halfway vector
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return num / denom;
}

// Geometry Function (Schlick-GGX)
// Describes self-shadowing of microfacets
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    
    return num / denom;
}

// Smith's method for geometry obstruction
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

// Fresnel-Schlick Approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    if (projCoords.z > 1.0) return 1.0;
    
    vec2 edgeDistance = min(projCoords.xy, 1.0 - projCoords.xy);
    float minEdgeDist = min(edgeDistance.x, edgeDistance.y);
    float fadeStart = 0.15;
    float edgeFade = smoothstep(0.0, fadeStart, minEdgeDist);
    
    if (projCoords.x < 0.0 || projCoords.x > 1.0 || 
        projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 1.0;
    }
    
    if (edgeFade < 0.01) return 1.0;
    
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;
    float bias = max(0.00005 * (1.0 - dot(normal, lightDir)), 0.0005);
    
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    shadow = mix(1.0, shadow, edgeFade);
    
    return shadow;
}

void main() {
    vec4 texColor = texture(albedoMap, TexCoord);
    
    if (texColor.a < 0.5) discard;
    
    // Sample textures
    vec3 albedo = hasAlbedoMap ? texture(albedoMap, TexCoord).rgb : baseColor;
    float metallicValue = hasMetallicMap ? texture(metallicMap, TexCoord).r : metallic;
    float roughnessValue = hasRoughnessMap ? texture(roughnessMap, TexCoord).r : roughness;
    float ao = hasAOMap ? texture(aoMap, TexCoord).r : 1.0;
    vec3 emissiveValue = hasEmissiveMap ? texture(emissiveMap, TexCoord).rgb : emissive;
    
    // Get normal from normal map or use vertex normal
    vec3 N;
    if (hasNormalMap) {
        // Sample normal map and transform from [0,1] to [-1,1]
        N = texture(normalMap, TexCoord).rgb;
        N = N * 2.0 - 1.0;
        N = normalize(TBN * N);  // Transform to world space
    } else {
        N = normalize(Normal);
    }
    
    vec3 V = normalize(viewPos - FragPos);
    
    // Calculate reflectance at normal incidence (F0)
    // For dielectrics (non-metals), F0 is typically 0.04
    // For metals, F0 is the albedo color
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallicValue);
    
    // Reflectance equation
    vec3 Lo = vec3(0.0);
    
    for (int i = 0; i < lightCount && i < MAX_LIGHTS; i++) {
        // Calculate per-light radiance
        vec3 L = normalize(lightPositions[i] - FragPos);
        vec3 H = normalize(V + L);
        float distance = length(lightPositions[i] - FragPos);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance = lightColors[i] * lightIntensities[i] * attenuation;
        
        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughnessValue);
        float G = GeometrySmith(N, V, L, roughnessValue);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        
        vec3 kS = F;  // Specular contribution
        vec3 kD = vec3(1.0) - kS;  // Diffuse contribution
        kD *= 1.0 - metallicValue;  // Metals have no diffuse
        
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;
        
        // Add to outgoing radiance Lo
        float NdotL = max(dot(N, L), 0.0);
        
        // Apply shadows for the shadow-casting light
        float shadow = 0.0;
        if (i == shadowLightIndex) {
            shadow = ShadowCalculation(FragPosLightSpace, N, L);
        }
        
        Lo += (kD * albedo / PI + specular) * radiance * NdotL * (1.0 - shadow);
    }
    
    // Ambient lighting
    vec3 ambient = vec3(0.03) * albedo * ao;
    
    vec3 color = ambient + Lo + emissiveValue;
    
    // HDR tonemapping (Reinhard)
    color = color / (color + vec3(1.0));
    
    // Gamma correction
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
    if (texColor.a < 0.5) {
        discard; // Prevents the fragment from writing to the depth buffer
    }
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
    GLuint cubemap_texture;
    std::unique_ptr<Shader> skybox_shader;
    
    Skybox() : VAO(0), VBO(0), cubemap_texture(0) {}
    
    ~Skybox() {
        cleanup();
    }
    
    void init(const char* faces[6]) {
        try {
            skybox_shader = std::make_unique<Shader>(skybox_vertex_shader, skybox_fragment_shader);
            printf("Skybox shaders created successfully. %u\n", skybox_shader->getProgram());
        } catch (const std::exception& e) {
            printf("Failed to create skybox shaders: %s\n", e.what());
            throw;
        }
        
        cubemap_texture = loadCubemap(faces);
        
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
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap_texture);
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
        if (cubemap_texture != 0) glDeleteTextures(1, &cubemap_texture);
        
        VAO = VBO = cubemap_texture = 0;
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
        
        glm::vec3 lightPos = light.position;
        
        // Look at scene center or camera
        glm::vec3 lightTarget = glm::vec3(0.0f, 0.0f, 0.0f);
        
        // Calculate light direction
        glm::vec3 lightDir = glm::normalize(lightTarget - lightPos);
        
        // Choose up vector dynamically
        glm::vec3 up;
        if (abs(lightDir.y) > 0.99f) {
            // Light is pointing nearly straight up or down
            // Use X axis as up vector instead
            up = glm::vec3(1.0f, 0.0f, 0.0f);
        } else {
            // Normal case: use Y axis as up
            up = glm::vec3(0.0f, 1.0f, 0.0f);
        }
        
        // Perspective projection for point lights
        glm::mat4 lightProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.5f, 100.0f);
        
        glm::mat4 lightView = glm::lookAt(lightPos, lightTarget, up);
        
        lightSpaceMatrix = lightProjection * lightView;
        
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
                for (Mesh* mesh : entity->meshes) {
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
                        GLuint texture_to_use = (mesh->texture_id != 0) ? mesh->texture_id : default_texture_id;
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
    
    void drawEntity(Entity* entity, const Camera& camera, const std::vector<Light>& lights, int shadowLightIndex) {
        if (!entity->active) return;
        
        for (Mesh* mesh : entity->meshes) {
            drawMesh(entity, mesh, camera, lights, shadowLightIndex);
        }
    }
    
    void drawMesh(Entity* entity, Mesh* mesh, const Camera& camera, const std::vector<Light>& lights, int shadowLightIndex) {
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
        
        // Set uniforms
        pbr_shader->setMat4("model", model);
        pbr_shader->setMat4("view", view);
        pbr_shader->setMat4("projection", projection);
        pbr_shader->setMat3("normalMatrix", normal);
        pbr_shader->setMat4("lightSpaceMatrix", lightSpaceMatrix);
        pbr_shader->setInt("shadowLightIndex", shadowLightIndex);
        
        // Set lighting
        int light_count = std::min(static_cast<int>(lights.size()), MAX_LIGHTS);
        pbr_shader->setInt("lightCount", light_count);
        pbr_shader->setVec3("viewPos", camera.position);
        
        std::vector<glm::vec3> positions, colors;
        std::vector<float> intensities;
        
        for (int i = 0; i < light_count; i++) {
            positions.push_back(lights[i].position);
            colors.push_back(lights[i].color);
            intensities.push_back(lights[i].intensity * 1000.0f);
        }
        
        pbr_shader->setVec3Array("lightPositions", positions);
        pbr_shader->setVec3Array("lightColors", colors);
        pbr_shader->setFloatArray("lightIntensities", intensities);
        
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
        
        // Metallic map
        glActiveTexture(GL_TEXTURE0 + texture_unit);
        GLuint metallic_to_use = mesh->material.hasMetallicMap() ?
        mesh->material.metallic_map : default_texture_id;
        glBindTexture(GL_TEXTURE_2D, metallic_to_use);
        pbr_shader->setInt("metallicMap", texture_unit);
        pbr_shader->setInt("hasMetallicMap", mesh->material.hasMetallicMap());
        texture_unit++;
        
        // Roughness map
        glActiveTexture(GL_TEXTURE0 + texture_unit);
        GLuint roughness_to_use = mesh->material.hasRoughnessMap() ?
        mesh->material.roughness_map : default_texture_id;
        glBindTexture(GL_TEXTURE_2D, roughness_to_use);
        pbr_shader->setInt("roughnessMap", texture_unit);
        pbr_shader->setInt("hasRoughnessMap", mesh->material.hasRoughnessMap());
        texture_unit++;
        
        // AO map
        glActiveTexture(GL_TEXTURE0 + texture_unit);
        GLuint ao_to_use = mesh->material.hasAOMap() ?
        mesh->material.ao_map : default_texture_id;
        glBindTexture(GL_TEXTURE_2D, ao_to_use);
        pbr_shader->setInt("aoMap", texture_unit);
        pbr_shader->setInt("hasAOMap", mesh->material.hasAOMap());
        texture_unit++;
        
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
    
    void drawUnlitMesh(Entity* entity, Mesh* mesh, const glm::vec3& color, float intensity = 1.0f) {
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
        unlit_shader->setFloat("emissiveIntensity", intensity * 0.001);
        
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

// Updated loadOBJWithMTL function (replace your existing one)
std::vector<Mesh*> loadOBJWithMTL(const std::string& obj_path) {
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
    std::vector<Mesh*> meshes;
    for (const auto& pair : unique_vertices) {
        const std::string& mat_name = pair.first;
        const std::vector<Vertex>& vertices = pair.second;
        const std::vector<unsigned int>& indices = indices_by_material[mat_name];

        Mesh* mesh = new Mesh();
        mesh->TRIANGLE_COUNT = static_cast<unsigned int>(indices.size() / 3);
        mesh->INDEX_COUNT = static_cast<unsigned int>(indices.size());
        mesh->material = materials.at(mat_name);
        mesh->texture_id = materials.at(mat_name).texture_id;

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

        meshes.push_back(mesh);
    }
    
    return meshes;
}

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

void loadMTL(const std::string& mtl_path, std::unordered_map<std::string, Material>& materials) {
    std::ifstream file(mtl_path);
    if (!file.is_open()) {
        std::cerr << "Warning: Cannot open MTL file " << mtl_path << std::endl;
        return;
    }

    std::string current_material_name;
    std::string line;
    
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string token;
        ss >> token;

        if (token == "newmtl") {
            ss >> current_material_name;
            materials[current_material_name] = createDefaultMaterial();
            materials[current_material_name].name = current_material_name;
            
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
            ss >> texture_filename;
            std::string mtl_dir = mtl_path.substr(0, mtl_path.find_last_of('/'));
            std::string full_texture_path = mtl_dir + "/" + texture_filename;
            
            parse_texture_map_line(ss, full_texture_path);
            materials[current_material_name].albedo_map = loadTexture(full_texture_path);
            printf("Loaded albedo map for '%s': %s\n", current_material_name.c_str(), texture_filename.c_str());
            
        } else if ((token == "map_Pr" || token == "map_roughness")
                   && materials.count(current_material_name)) {
            // Roughness map
            std::string texture_filename;
            ss >> texture_filename;
            std::string mtl_dir = mtl_path.substr(0, mtl_path.find_last_of('/'));
            std::string full_texture_path = mtl_dir + "/" + texture_filename;
            
            parse_texture_map_line(ss, full_texture_path);
            materials[current_material_name].roughness_map = loadTexture(full_texture_path);
            printf("Loaded roughness map for '%s': %s\n", current_material_name.c_str(), texture_filename.c_str());
            
        } else if ((token == "map_Pm" || token == "map_metallic")
                   && materials.count(current_material_name)) {
            // Metallic map
            std::string texture_filename;
            ss >> texture_filename;
            std::string mtl_dir = mtl_path.substr(0, mtl_path.find_last_of('/'));
            std::string full_texture_path = mtl_dir + "/" + texture_filename;
            
            parse_texture_map_line(ss, full_texture_path);
            materials[current_material_name].metallic_map = loadTexture(full_texture_path);
            printf("Loaded metallic map for '%s': %s\n", current_material_name.c_str(), texture_filename.c_str());
            
        } else if ((token == "norm" || token == "map_Bump" || token == "bump")
                   && materials.count(current_material_name)) {
            // Normal map
            std::string texture_filename;
            ss >> texture_filename;
            std::string mtl_dir = mtl_path.substr(0, mtl_path.find_last_of('/'));
            std::string full_texture_path = mtl_dir + "/" + texture_filename;
            
            parse_texture_map_line(ss, full_texture_path);
            materials[current_material_name].normal_map = loadTexture(full_texture_path);
            printf("Loaded normal map for '%s': %s\n", current_material_name.c_str(), texture_filename.c_str());
            
        } else if (token == "map_ao" && materials.count(current_material_name)) {
            // Ambient occlusion map
            std::string texture_filename;
            ss >> texture_filename;
            std::string mtl_dir = mtl_path.substr(0, mtl_path.find_last_of('/'));
            std::string full_texture_path = mtl_dir + "/" + texture_filename;
            
            parse_texture_map_line(ss, full_texture_path);
            materials[current_material_name].ao_map = loadTexture(full_texture_path);
            printf("Loaded AO map for '%s': %s\n", current_material_name.c_str(), texture_filename.c_str());
            
        } else if ((token == "map_Ke" || token == "map_emissive")
                   && materials.count(current_material_name)) {
            // Emissive map
            std::string texture_filename;
            ss >> texture_filename;
            std::string mtl_dir = mtl_path.substr(0, mtl_path.find_last_of('/'));
            std::string full_texture_path = mtl_dir + "/" + texture_filename;
            
            parse_texture_map_line(ss, full_texture_path);
            materials[current_material_name].emissive_map = loadTexture(full_texture_path);
            printf("Loaded emissive map for '%s': %s\n", current_material_name.c_str(), texture_filename.c_str());
            
        } else if (token == "map_metallic_roughness" && materials.count(current_material_name)) {
            // Combined metallic-roughness map (glTF style: R = unused, G = roughness, B = metallic)
            std::string texture_filename;
            ss >> texture_filename;
            std::string mtl_dir = mtl_path.substr(0, mtl_path.find_last_of('/'));
            std::string full_texture_path = mtl_dir + "/" + texture_filename;
            
            parse_texture_map_line(ss, full_texture_path);
            materials[current_material_name].metallic_roughness_map = loadTexture(full_texture_path);
            printf("Loaded metallic-roughness map for '%s': %s\n", current_material_name.c_str(), texture_filename.c_str());
        }
    }
}

std::vector<Mesh*> loadOBJMesh(const std::string& obj_path) {
    const std::string& full_obj_path = buildAssetPath("OBJ_Models/" + obj_path);
    if (!std::filesystem::exists(full_obj_path)) {
        std::cerr << "ERROR::OBJ_LOAD::File does not exist: " << full_obj_path << std::endl;
        
        // File is missing or permission denied
        std::cerr << "Check your file path or OS file permissions." << std::endl;
        
        // Return empty mesh
        return {};
    }
    
    std::vector<Mesh*> meshes = loadOBJWithMTL(full_obj_path);

    if (meshes.empty()) {
        std::cerr << "Failed to load OBJ file: " << full_obj_path << std::endl;
        return {};
    } else {
        std::cerr << "Successfully loaded OBJ file: " << full_obj_path << std::endl;
    }

    entity_count++;
    return meshes;
}

int main() {
    
    // ============================================================================
    // INITIALIZER
    // ============================================================================
    
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
    
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        printf("Failed to initialize GLAD\n");
        return -1;
    }
    
    printf("OpenGL Version: %s\n", glGetString(GL_VERSION));
    
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
    
    Skybox skybox;
    skybox.init(cloud_skybox);
    
    // LOAD OBJ MESHES //
    
    // Note that when importing new assets you only need to add your files to the main assets folders
    // You can find the main asset folders at "OpenGL 3D Engine/OpenGL 3D Engine/OBJ_Models" or "OpenGL 3D Engine/OpenGL 3D Engine/Skyboxes"
    
    std::vector<Mesh*> level_mesh = loadOBJMesh("Level/level.obj");
    std::vector<Mesh*> tree_mesh = loadOBJMesh("Realistic_tree/tree.obj");
    std::vector<Mesh*> instructions_mesh = loadOBJMesh("Instructions_Panel/quad.obj");
    std::vector<Mesh*> cube_mesh = loadOBJMesh("Cube/cube.obj");
    std::vector<Mesh*> sphere_mesh = loadOBJMesh("Sphere/sphere.obj");
    std::vector<Mesh*> streetlight_mesh = loadOBJMesh("Streetlight/streetlight.obj");
    std::vector<Mesh*> cone_mesh = loadOBJMesh("Cone/cone.obj");
    
    // ============================================================================
    // CREATE SCENE OBJECTS
    // ============================================================================
    
    // CREATE LIGHT SOURCES //
    
    createPointLight("light", glm::vec3(-7.05, 3.5, 0.05), glm::vec3(1, 1, 1), 1.0f, {}, glm::vec3(0.25, 0.25, 0.25), glm::vec3(0, -0.25, 0), std::vector<int>{CULL_BACK});
    
    // CREATE ENTITIES //
        
    createEntity("level", level_mesh, glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), glm::vec3(10, 10, 10), std::vector<int> {CULL_NONE});
    createEntity("tree", tree_mesh, glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), std::vector<int>{CULL_NONE, CULL_BACK});
    createEntity("instructions", instructions_mesh, glm::vec3(0, 2, 4), glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), std::vector<int> {CULL_NONE});
    createEntity("cube", cube_mesh, glm::vec3(5, 3, 0), glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), std::vector<int> {CULL_BACK});
    createEntity("sphere", sphere_mesh, glm::vec3(0, 2, -5), glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), std::vector<int> {CULL_BACK});
    createEntity("streetlight", streetlight_mesh, glm::vec3(-7, 0.05, 0), glm::vec3(1, 1, 1), glm::vec3(1, 1, 1), std::vector<int>{CULL_BACK});
    
    printf("Total triangles: %d\n", total_triangles);
    printf("Active entities: %zu\n", entity_manager.size());
    
    // ============================================================================
    // MAIN LOOP
    // ============================================================================
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        updateFPS(window);
        
        if (!paused) {
            
            // ============================================================================
            // UPDATE SCENE OBJECTS
            // ============================================================================
            
            // UPDATE LIGHTS
            // lights[0].position = global_camera.position;
            
            // UPDATE OBJECTS
            entity_manager.updateEntity("cube", VEC3_NO_CHANGE, glm::vec3(update_count, update_count * 0.5f, 0), VEC3_NO_CHANGE);
            entity_manager.updateEntity("sphere", glm::vec3(0, 2.5 + sinf(update_count * 0.01f), -5), glm::vec3(update_count, 0, 0), VEC3_NO_CHANGE);
            
            update_count += speed_multiplier;
            
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
        }
        
        // Clear background
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
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
                        for (Mesh* mesh : entity->meshes) {
                            if (mesh && mesh->isValid()) {
                                renderer->drawUnlitMesh(entity, mesh, light.color, light.intensity);
                            }
                        }
                        break;
                    }
                }
                
                // Render normally if not a light
                if (!is_light) {
                    renderer->drawEntity(entity, global_camera, lights, shadowLightIndex);
                }
            }
        }
        
        glfwSwapBuffers(window);
        
        // Handle pause/unpause
        if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_NORMAL) {
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) {
                paused = false;
                firstMouse = true;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // Enable pointerlock
            }
        } else {
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                paused = true;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); // Disable pointerlock
            }
        }
    }
    
    // ============================================================================
    // CLEANUP
    // ============================================================================
    
    printf("Cleaning up...\n");
    cleanup_all_meshes();
    skybox.cleanup();
    
    if (default_texture_id != 0) {
        glDeleteTextures(1, &default_texture_id);
    }
    
    cleanupShadowMap();
    
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

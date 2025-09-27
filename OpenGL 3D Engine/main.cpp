//
//  main.cpp
//  OpenGL 3D Engine
//
//  Created by Ray Hsiao Muguang on 2025/9/15.
//

/* TO-DO LIST
 
 1. Add face culling system choices for each submesh
 
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
int window_width = 800;
int window_height = 600;

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
    std::filesystem::path exe_path = std::filesystem::current_path();
    std::filesystem::path project_root = exe_path;
    
    while (!std::filesystem::exists(project_root / "OBJ_Models") &&
           project_root != project_root.parent_path()) {
        project_root = project_root.parent_path();
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

class Material {
public:
    std::string name;
    GLuint texture_id = 0;
    Color diffuse_color;
    glm::vec3 ambient{0.1f, 0.1f, 0.1f};
    glm::vec3 diffuse{0.8f, 0.8f, 0.8f};
    glm::vec3 specular{0.5f, 0.5f, 0.5f};
    float shininess = 32.0f;
    
    Material() = default;
    Material(const std::string& name) : name(name) {}
};

Material createDefaultMaterial() {
    Material default_mat;
    default_mat.name = "default";
    default_mat.texture_id = default_texture_id;
    default_mat.diffuse_color = {1.0f, 1.0f, 1.0f, 1.0f};
    return default_mat;
}

// ============================================================================
// TEXTURE LOADING
// ============================================================================

GLuint createDefaultTexture() {
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    
    constexpr unsigned char white_pixel[4] = {255, 255, 255, 255};
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

// ============================================================================
// LIGHTING SYSTEM
// ============================================================================

typedef struct {
    glm::vec3 position;
    glm::vec3 color;
    float intensity;
} Light;

std::vector<Light> lights;

void createLightSource(glm::vec3 position, glm::vec3 color, float intensity) {
    Light light;
    light.position = position;
    light.color = color;
    light.intensity = intensity;
    lights.push_back(light);
}

// ============================================================================
// MESH SYSTEM
// ============================================================================

typedef struct { float u, v; } Vec2;
typedef enum { CULL_NONE = 0, CULL_BACK = 1, CULL_FRONT = 2 } CullMode;

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
    CullMode cull_mode;
    bool is_cleaned_up;
    
    Mesh() : vertex_count(0), index_count(0), TRIANGLE_COUNT(0),
             VAO(0), VBO(0), EBO(0), texture_id(0),
             cull_mode(CULL_BACK), is_cleaned_up(false) {
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
    
    bool updateEntity(const char* name, const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scale) {
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
    
    void compactEntities() {
        // Remove inactive entities
    }
};

EntityManager entity_manager;

void createEntity(const char* name, std::vector<Mesh*> meshes, glm::vec3 pos, glm::vec3 rotation, glm::vec3 scale) {
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
    
    unsigned int mesh_triangles = 0;
    for (Mesh* mesh : meshes) {
        if (mesh) mesh_triangles += mesh->TRIANGLE_COUNT;
    }
    
    total_triangles += mesh_triangles;
    
    printf("Created entity '%s' with %zu submesh(es) (total triangles: %u)\n",
           name, meshes.size(), mesh_triangles);
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
    cam->front = normalize(cam->front);

    cam->right = normalize(cross(cam->front, glm::vec3(0, 1, 0)));
    cam->up = normalize(cross(cam->right, cam->front));
}

glm::mat4 camera_get_projection(Camera* cam) {
    return glm::perspective(cam->fov, cam->aspect_ratio, cam->near_plane, cam->far_plane);
}

glm::mat4 camera_get_view_matrix(Camera* cam) {
    return lookAt(cam->position, cam->position + cam->front, cam->up);
}

// ============================================================================
// SHADER SYSTEM
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
        glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, value_ptr(value));
    }
    
    void setMat3(const std::string& name, const glm::mat3& value) const {
        glUniformMatrix3fv(getUniformLocation(name), 1, GL_FALSE, value_ptr(value));
    }
    
    void setVec3(const std::string& name, const glm::vec3& value) const {
        glUniform3fv(getUniformLocation(name), 1, value_ptr(value));
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
// SHADER SOURCES
// ============================================================================

const std::string vertex_shader_source = R"(
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aNormal;

out vec4 vertexColor;
out vec2 TexCoord;
out vec3 FragPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = normalMatrix * aNormal;
    gl_Position = projection * view * vec4(FragPos, 1.0);
    TexCoord = aTexCoords;
    vertexColor = aColor;
}
)";

const std::string fragment_shader_source = R"(
#version 330 core

in vec4 vertexColor;
in vec2 TexCoord;
in vec3 FragPos;
in vec3 Normal;
out vec4 FragColor;

uniform sampler2D u_texture;

#define MAX_LIGHTS 16
uniform vec3 lightPositions[MAX_LIGHTS];
uniform vec3 lightColors[MAX_LIGHTS];
uniform float lightIntensities[MAX_LIGHTS];
uniform int lightCount;
uniform vec3 viewPos;

uniform vec3 materialAmbient;
uniform vec3 materialDiffuse;
uniform vec3 materialSpecular;
uniform float materialShininess;

void main() {
    vec4 texColor = texture(u_texture, TexCoord);
    
    if (texColor.a < 0.1) {
        discard;
    }
    
    vec3 norm = normalize(Normal);
    vec3 finalLighting = vec3(0.0);
    float face_mask = gl_FrontFacing ? 1.0 : 0.0; 

    for (int i = 0; i < lightCount && i < MAX_LIGHTS; i++) {
        // Ambient
        vec3 ambient = materialAmbient * lightColors[i];
        
        vec3 lightDir = normalize(lightPositions[i] - FragPos);
        vec3 correctedNorm = norm;
        if (dot(correctedNorm, lightDir) < 0.0) {
            correctedNorm = -correctedNorm; // Flip the normal vector
        }
        
        // Diffuse
        float diff_intensity = max(dot(correctedNorm, lightDir), 0.0);
        // Apply face_mask: Only light the 'front' side of the plane
        vec3 diffuse = diff_intensity * face_mask * materialDiffuse * lightColors[i];

        // Specular
        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec_intensity = pow(max(dot(correctedNorm, halfwayDir), 0.0), materialShininess);
        vec3 specular = spec_intensity * face_mask * materialSpecular * lightColors[i];

        finalLighting += (ambient + diffuse + specular) * lightIntensities[i];
    }

    FragColor = vec4(finalLighting, 1.0) * texColor * vertexColor;
}
)";

// ============================================================================
// RENDERER CLASS
// ============================================================================

class Renderer {
private:
    std::unique_ptr<Shader> main_shader;
    
public:
    Renderer() {
        try {
            main_shader = std::make_unique<Shader>(vertex_shader_source, fragment_shader_source);
            printf("Shader created successfully. Program ID: %u\n", main_shader->getProgram());
        } catch (const std::exception& e) {
            printf("Failed to create shader: %s\n", e.what());
            throw;
        }
    }
    
    void drawEntity(Entity* entity, const Camera& camera, const std::vector<Light>& lights) {
        if (!entity->active) return;
        
        // Render all meshes of this entity with the same attributes but different materials
        for (Mesh* mesh : entity->meshes) {
            if (mesh && mesh->isValid()) {
                drawMesh(entity, mesh, camera, lights);
            }
        }
    }
    
    void drawMesh(Entity* entity, Mesh* mesh, const Camera& camera, const std::vector<Light>& lights) {
        if (!entity->active || mesh->TRIANGLE_COUNT == 0 || !mesh->isValid()) {
            return;
        }
        
        if (mesh->VAO == 0 || mesh->VBO == 0) {
            printf("Error: Mesh has an invalid VAO (%u) or VBO (%u).\n", mesh->VAO, mesh->VBO);
            return;
        }
        
        main_shader->use();
        
        // Calculate matrices
        glm::mat4 scale_matrix = scale(glm::mat4(1.0f), entity->scale);
        glm::mat4 rotation_x = rotate(glm::mat4(1.0f), glm::radians(entity->rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 rotation_y = rotate(glm::mat4(1.0f), glm::radians(entity->rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 rotation_z = rotate(glm::mat4(1.0f), glm::radians(entity->rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        glm::mat4 rotation = rotation_z * rotation_y * rotation_x;
        glm::mat4 translation = translate(glm::mat4(1.0f), entity->position);
                
        glm::mat4 model = translation * rotation * scale_matrix;
        glm::mat3 normal = transpose(inverse(glm::mat3(model)));
        
        // Set uniforms
        main_shader->setMat4("model", model);
        main_shader->setMat4("view", view);
        main_shader->setMat4("projection", projection);
        main_shader->setMat3("normalMatrix", normal);
        
        // Set lighting
        int light_count = std::min(static_cast<int>(lights.size()), MAX_LIGHTS);
        main_shader->setInt("lightCount", light_count);
        main_shader->setVec3("viewPos", camera.position);
        
        std::vector<glm::vec3> positions, colors;
        std::vector<float> intensities;
        
        for (int i = 0; i < light_count; i++) {
            positions.push_back(lights[i].position);
            colors.push_back(lights[i].color);
            intensities.push_back(lights[i].intensity);
        }
        
        main_shader->setVec3Array("lightPositions", positions);
        main_shader->setVec3Array("lightColors", colors);
        main_shader->setFloatArray("lightIntensities", intensities);
        
        // Set material
        main_shader->setVec3("materialAmbient", mesh->material.ambient);
        main_shader->setVec3("materialDiffuse", mesh->material.diffuse);
        main_shader->setVec3("materialSpecular", mesh->material.specular);
        main_shader->setFloat("materialShininess", mesh->material.shininess);
                
        // Set texture
        GLuint texture_to_use = (mesh->texture_id != 0) ? mesh->texture_id : default_texture_id;
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture_to_use);
        main_shader->setInt("u_texture", 0);
        
        // Draw
        glBindVertexArray(mesh->VAO);
        glDrawElements(GL_TRIANGLES, mesh->INDEX_COUNT, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
};

// ============================================================================
// OBJ AND MTL LOADERS
// ============================================================================

void loadMTL(const std::string& mtl_path, std::unordered_map<std::string, Material>& materials);

struct Vertex {
    glm::vec3 position;
    glm::vec4 color;
    glm::vec2 texcoord;
    glm::vec3 normal;

    bool operator==(const Vertex& other) const {
        return position == other.position &&
               texcoord == other.texcoord &&
               normal == other.normal &&
               color == other.color;
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
                 ^ (hash<float>()(v.normal.z) << 6);
        }
    };
}

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
            break; // We only need the first mtllib line
        }
    }
    file.clear(); // Clear any error flags
    file.seekg(0, std::ios::beg); // Rewind to the beginning of the file

    // If no materials were loaded, use a default material
    if (materials.empty()) {
        materials["default"] = createDefaultMaterial();
        current_material_name = "default";
    }

    std::vector<glm::vec3> temp_vertices;
    std::vector<glm::vec2> temp_texcoords;
    std::vector<glm::vec3> temp_normals;
    
    std::unordered_map<std::string, std::vector<float>> vertices_by_material;

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
            std::string v1_str, v2_str, v3_str, v4_str;
            ss >> v1_str >> v2_str >> v3_str >> v4_str;

            // Process first triangle of face
            auto process_vertex = [&](const std::string& v_str, const std::string& mat_name) {
                std::stringstream v_ss(v_str);
                std::string v, vt, vn;
                std::getline(v_ss, v, '/');
                std::getline(v_ss, vt, '/');
                std::getline(v_ss, vn, '/');

                int v_idx = std::stoi(v) - 1;
                int vt_idx = std::stoi(vt) - 1;
                int vn_idx = std::stoi(vn) - 1;

                Vertex vertex;
                vertex.position = temp_vertices[v_idx];
                vertex.texcoord = temp_texcoords[vt_idx];
                vertex.normal = temp_normals[vn_idx];
                vertex.color = materials[mat_name].diffuse_color.toVec4();

                auto& v_map = vertex_to_index[mat_name];
                auto& verts = unique_vertices[mat_name];
                auto& inds = indices_by_material[mat_name];

                if (v_map.count(vertex) == 0) {
                    v_map[vertex] = static_cast<unsigned int>(verts.size());
                    verts.push_back(vertex);
                }

                inds.push_back(v_map[vertex]);
            };

            process_vertex(v1_str, current_material_name);
            process_vertex(v2_str, current_material_name);
            process_vertex(v3_str, current_material_name);
            if (!v4_str.empty()) {
                process_vertex(v1_str, current_material_name);
                process_vertex(v3_str, current_material_name);
                process_vertex(v4_str, current_material_name);
            }

        }
    }

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
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, position));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, color));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, texcoord));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(3);

        glBindVertexArray(0);

        meshes.push_back(mesh);
    }
    
    return meshes;
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
        } else if (token == "Ka" && materials.count(current_material_name)) {
            ss >> materials[current_material_name].ambient.x >> materials[current_material_name].ambient.y >> materials[current_material_name].ambient.z;
        } else if (token == "Kd" && materials.count(current_material_name)) {
            ss >> materials[current_material_name].diffuse.x >> materials[current_material_name].diffuse.y >> materials[current_material_name].diffuse.z;
            materials[current_material_name].diffuse_color = {materials[current_material_name].diffuse.x, materials[current_material_name].diffuse.y, materials[current_material_name].diffuse.z, 1.0f};
        } else if (token == "Ks" && materials.count(current_material_name)) {
            ss >> materials[current_material_name].specular.x >> materials[current_material_name].specular.y >> materials[current_material_name].specular.z;
        } else if (token == "Ns" && materials.count(current_material_name)) {
            ss >> materials[current_material_name].shininess;
        } else if (token == "map_Kd" && materials.count(current_material_name)) {
            std::string texture_filename;
            ss >> texture_filename;
            std::string mtl_dir = mtl_path.substr(0, mtl_path.find_last_of('/'));
            std::string full_texture_path = mtl_dir + "/" + texture_filename;
            materials[current_material_name].texture_id = loadTexture(full_texture_path);
            if (materials[current_material_name].texture_id == 0) {
                std::cerr << "Warning: Failed to load texture " << full_texture_path << " for material " << current_material_name << std::endl;
            }
        }
    }
}

std::vector<Mesh*> loadOBJMesh(const std::string& obj_path) {
    std::vector<Mesh*> meshes = loadOBJWithMTL(obj_path);

    if (meshes.empty()) {
        std::cerr << "Failed to load OBJ file: " << obj_path << std::endl;
        return {};
    } else {
        std::cerr << "Successfully loaded OBJ file: " << obj_path << std::endl;
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
    
    GLFWwindow* window = glfwCreateWindow(window_width, window_height, "OpenGL 3D Engine", NULL, NULL);
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
    
    // Initialize camera
    global_camera = create_camera(static_cast<float>(window_width) / static_cast<float>(window_height));
    camera_update_vectors(&global_camera);
        
    // Z buffer
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // OpenGL pixel functions
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_MULTISAMPLE);
    
    // ============================================================================
    // CREATE SCENE OBJECTS
    // ============================================================================
    
    // CREATE LIGHT SOURCES //
    
    createLightSource(glm::vec3(0, 10, 0), glm::vec3(1, 1, 1), 1.0f);
        
    // LOAD OBJ MESHES //
    
    std::vector<Mesh*> level_mesh = loadOBJMesh(buildAssetPath("OBJ_Models/Level/level.obj"));
    std::vector<Mesh*> tree_mesh = loadOBJMesh(buildAssetPath("OBJ_Models/Realistic_tree/tree.obj"));
    std::vector<Mesh*> instructions_mesh = loadOBJMesh(buildAssetPath("OBJ_Models/Instructions_Panel/quad.obj"));
    std::vector<Mesh*> cube_mesh = loadOBJMesh(buildAssetPath("OBJ_Models/Cube/cube.obj"));
    
    // CREATE ENTITIES //
    
    createEntity("level", level_mesh, glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), glm::vec3(10, 10, 10));
    createEntity("tree", tree_mesh, glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), glm::vec3(1, 1, 1));
    createEntity("instructions", instructions_mesh, glm::vec3(0, 2, 4), glm::vec3(0, 0, 0), glm::vec3(1, 1, 1));
    createEntity("cube", cube_mesh, glm::vec3(5, 3, 0), glm::vec3(0, 0, 0), glm::vec3(1, 1, 1));
    
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
                cam_offset = normalize(cam_offset);
            }
            global_camera.position = global_camera.position + cam_offset * camera_speed;
            
            // Update camera matrices
            view = camera_get_view_matrix(&global_camera);
            projection = camera_get_projection(&global_camera);
        }
        
        // Clear background
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        
        // Render all entities
        for (size_t i = 0; i < entity_manager.size(); i++) {
            Entity* entity = entity_manager.getEntityAt(i);
            if (entity && entity->active) {
                renderer->drawEntity(entity, global_camera, lights);
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
    
    if (default_texture_id != 0) {
        glDeleteTextures(1, &default_texture_id);
    }
    
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
    window_width = width;
    window_height = height;
    (void)window;
    glViewport(0, 0, width, height);
    if (height > 0) {
        global_camera.aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
    }
}

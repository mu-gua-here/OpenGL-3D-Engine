//
//  main.cpp
//  OpenGL 3D Engine
//
//  Created by Ray Hsiao Muguang on 2025/9/15.
//

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

double fps;
bool paused = false;
unsigned int total_triangles = 0;
unsigned int entity_count = 0;
float update_count = 0;
float frame_time = 1.0f;
float speed_multiplier = 1.0f;

GLuint default_texture_id = 0;
glm::mat4 view;
glm::mat4 projection;

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
            "C++ OpenGL 3D Engine - FPS: %.1f | Frame time: %.3f ms",
            fps, frame_time * 1000.0);
        glfwSetWindowTitle(window, title);
        frameCount = 0;
        fpsTimer = 0.0;
    }
}

// ============================================================================
// FILE MANAGEMENT
// ============================================================================

std::string getProjectRoot() {
    std::filesystem::path exe_path = std::filesystem::current_path();
    std::filesystem::path project_root = exe_path;
    
    while (!std::filesystem::exists(project_root / "OBJ_models") &&
           project_root != project_root.parent_path()) {
        project_root = project_root.parent_path();
    }
    
    return project_root.string();
}

std::string buildAssetPath(const std::string& relative_path) {
    static std::string project_root = getProjectRoot();
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
    default_mat.ambient = glm::vec3(0.1f);
    default_mat.diffuse = glm::vec3(0.8f);
    default_mat.specular = glm::vec3(0.5f);
    default_mat.shininess = 32.0f;
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
        internalFormat = GL_R8; dataFormat = GL_RED;
    } else if (channels == 3) {
        internalFormat = GL_RGB8; dataFormat = GL_RGB;
    } else if (channels == 4) {
        internalFormat = GL_RGBA8; dataFormat = GL_RGBA;
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

void create_light_source(glm::vec3 position, glm::vec3 color, float intensity) {
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
        
        vertex_count = index_count = TRIANGLE_COUNT = 0;
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
    Mesh* mesh;
    int active;
} Entity;

Entity create_entity(const char* name, Mesh* mesh, glm::vec3 pos, glm::vec3 rotation, glm::vec3 scale) {
    Entity entity;
    entity.name = name;
    entity.position = pos;
    entity.rotation.x = rotation.x * M_PI / 180.0f;
    entity.rotation.y = rotation.y * M_PI / 180.0f;
    entity.rotation.z = rotation.z * M_PI / 180.0f;
    entity.scale = scale;
    entity.mesh = mesh;
    entity.active = 1;
    return entity;
}

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
                    entities[i].rotation.x = rot.x * M_PI / 180.0f;
                    entities[i].rotation.y = rot.y * M_PI / 180.0f;
                    entities[i].rotation.z = rot.z * M_PI / 180.0f;
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
        // Simple implementation - just remove inactive entities
    }
};

EntityManager entity_manager;

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
    cam.position = glm::vec3(0, 2, 5);  // Move camera back to see objects
    cam.front = glm::vec3(0, 0, -1);
    cam.up = glm::vec3(0, 1, 0);
    cam.right = glm::vec3(1, 0, 0);
    cam.yaw = -90.0f;
    cam.pitch = -10.0f;  // Look slightly down
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
    glm::vec3 target = cam->position + cam->front;
    return lookAt(cam->position, target, cam->up);
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

#define MAX_LIGHTS 8
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
    
    if (lightCount == 0) {
        vec3 ambient = materialAmbient * 0.5;
        FragColor = vec4(ambient, 1.0) * texColor * vertexColor;
        return;
    }
    
    vec3 norm = normalize(Normal);
    vec3 finalLighting = vec3(0.0);

    for (int i = 0; i < lightCount && i < MAX_LIGHTS; ++i) {
        vec3 ambient = materialAmbient * lightColors[i] * 0.1;
        
        vec3 lightDir = normalize(lightPositions[i] - FragPos);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * materialDiffuse * lightColors[i];

        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(norm, halfwayDir), 0.0), materialShininess);
        vec3 specular = spec * materialSpecular * lightColors[i];

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
    
    void drawMesh(Entity* entity, Mesh* mesh, const Camera& camera, const std::vector<Light>& lights) {
        if (!entity->active || mesh->TRIANGLE_COUNT == 0 || !mesh->isValid()) {
            return;
        }
        
        main_shader->use();
        
        // Calculate matrices
        glm::mat4 scale_matrix = scale(glm::mat4(1.0f), entity->scale);
        glm::mat4 rotation_x = rotate(glm::mat4(1.0f), entity->rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 rotation_y = rotate(glm::mat4(1.0f), entity->rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 rotation_z = rotate(glm::mat4(1.0f), entity->rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        glm::mat4 rotation = rotation_z * rotation_y * rotation_x;
        glm::mat4 translation = translate(glm::mat4(1.0f), entity->position);
        glm::mat4 model = translation * rotation * scale_matrix;
        
        glm::mat4 view_matrix = camera_get_view_matrix(const_cast<Camera*>(&camera));
        glm::mat4 projection_matrix = camera_get_projection(const_cast<Camera*>(&camera));
        glm::mat3 normal_matrix = transpose(inverse(glm::mat3(model)));
        
        // Set uniforms
        main_shader->setMat4("model", model);
        main_shader->setMat4("view", view_matrix);
        main_shader->setMat4("projection", projection_matrix);
        main_shader->setMat3("normalMatrix", normal_matrix);
        
        // Set lighting
        int light_count = std::min(static_cast<int>(lights.size()), 8);
        main_shader->setInt("lightCount", light_count);
        main_shader->setVec3("viewPos", camera.position);
        
        if (light_count > 0) {
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
        }
        
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
        glDrawArrays(GL_TRIANGLES, 0, mesh->TRIANGLE_COUNT * 3);
        glBindVertexArray(0);
    }
};

// ============================================================================
// SIMPLE MESH CREATION FOR TESTING
// ============================================================================

Mesh* createTestCube() {
    float cube_vertices[] = {
        // Front face - positions(3) + colors(4) + uvs(2) + normals(3) = 12 floats per vertex
        -1.0f, -1.0f,  1.0f,  1.0f, 0.0f, 0.0f, 1.0f,  0.0f, 0.0f,  0.0f, 0.0f, 1.0f,
         1.0f, -1.0f,  1.0f,  1.0f, 0.0f, 0.0f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f,
         1.0f,  1.0f,  1.0f,  1.0f, 0.0f, 0.0f, 1.0f,  1.0f, 1.0f,  0.0f, 0.0f, 1.0f,
         1.0f,  1.0f,  1.0f,  1.0f, 0.0f, 0.0f, 1.0f,  1.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        -1.0f,  1.0f,  1.0f,  1.0f, 0.0f, 0.0f, 1.0f,  0.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        -1.0f, -1.0f,  1.0f,  1.0f, 0.0f, 0.0f, 1.0f,  0.0f, 0.0f,  0.0f, 0.0f, 1.0f,
        
        // Back face
        -1.0f, -1.0f, -1.0f,  0.0f, 1.0f, 0.0f, 1.0f,  0.0f, 0.0f,  0.0f, 0.0f, -1.0f,
         1.0f, -1.0f, -1.0f,  0.0f, 1.0f, 0.0f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f, -1.0f,
         1.0f,  1.0f, -1.0f,  0.0f, 1.0f, 0.0f, 1.0f,  1.0f, 1.0f,  0.0f, 0.0f, -1.0f,
         1.0f,  1.0f, -1.0f,  0.0f, 1.0f, 0.0f, 1.0f,  1.0f, 1.0f,  0.0f, 0.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,  0.0f, 1.0f, 0.0f, 1.0f,  0.0f, 1.0f,  0.0f, 0.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,  0.0f, 1.0f, 0.0f, 1.0f,  0.0f, 0.0f,  0.0f, 0.0f, -1.0f,
    };
    
    Mesh* mesh = new Mesh();
    mesh->TRIANGLE_COUNT = 4; // 2 triangles per face, 2 faces for simplicity
    mesh->vertex_count = 12 * 12; // 12 vertices * 12 floats per vertex
    
    // Set vertices using vector
    std::vector<float> vertices_vec(cube_vertices, cube_vertices + mesh->vertex_count);
    mesh->setVertices(vertices_vec);
    
    // Create OpenGL objects
    glGenVertexArrays(1, &mesh->VAO);
    glGenBuffers(1, &mesh->VBO);
    
    glBindVertexArray(mesh->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, mesh->VBO);
    glBufferData(GL_ARRAY_BUFFER, mesh->vertex_count * sizeof(float), mesh->vertices_data.data(), GL_STATIC_DRAW);
    
    // Vertex attributes: position(3) + color(4) + texCoord(2) + normal(3) = 12 floats
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(7 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(9 * sizeof(float)));
    glEnableVertexAttribArray(3);
    
    glBindVertexArray(0);
    
    mesh->texture_id = default_texture_id;
    mesh->material = createDefaultMaterial();
    
    printf("Created test cube mesh with %u triangles\n", mesh->TRIANGLE_COUNT);
    return mesh;
}

// ============================================================================
// OBJ AND MTL LOADERS
// ============================================================================

void loadMTL(const std::string& mtl_path, std::unordered_map<std::string, Material>& materials);

std::vector<Mesh*> loadOBJWithMTL(const std::string& obj_path) {
    std::ifstream file(obj_path);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << obj_path << std::endl;
        return {};
    }

    std::unordered_map<std::string, Material> materials;
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
            auto process_vertex = [&](const std::string& v_str) {
                std::stringstream v_ss(v_str);
                std::string v, vt, vn;
                std::getline(v_ss, v, '/');
                std::getline(v_ss, vt, '/');
                std::getline(v_ss, vn, '/');
                
                int v_idx = std::stoi(v) - 1;
                int vt_idx = std::stoi(vt) - 1;
                int vn_idx = std::stoi(vn) - 1;
                
                vertices_by_material[current_material_name].push_back(temp_vertices[v_idx].x);
                vertices_by_material[current_material_name].push_back(temp_vertices[v_idx].y);
                vertices_by_material[current_material_name].push_back(temp_vertices[v_idx].z);

                vertices_by_material[current_material_name].push_back(materials[current_material_name].diffuse_color.r);
                vertices_by_material[current_material_name].push_back(materials[current_material_name].diffuse_color.g);
                vertices_by_material[current_material_name].push_back(materials[current_material_name].diffuse_color.b);
                vertices_by_material[current_material_name].push_back(materials[current_material_name].diffuse_color.a);
                
                vertices_by_material[current_material_name].push_back(temp_texcoords[vt_idx].x);
                vertices_by_material[current_material_name].push_back(temp_texcoords[vt_idx].y);

                vertices_by_material[current_material_name].push_back(temp_normals[vn_idx].x);
                vertices_by_material[current_material_name].push_back(temp_normals[vn_idx].y);
                vertices_by_material[current_material_name].push_back(temp_normals[vn_idx].z);
            };

            process_vertex(v1_str);
            process_vertex(v2_str);
            process_vertex(v3_str);

            if (!v4_str.empty()) { // Handle quads
                process_vertex(v1_str);
                process_vertex(v3_str);
                process_vertex(v4_str);
            }
        }
    }

    std::vector<Mesh*> meshes;
    for (const auto& pair : vertices_by_material) {
        const std::string& mat_name = pair.first;
        const std::vector<float>& vertices = pair.second;
        
        Mesh* mesh = new Mesh();
        mesh->setVertices(vertices);
        mesh->TRIANGLE_COUNT = static_cast<unsigned int>(vertices.size() / 12 / 3);
        mesh->material = materials.at(mat_name);
        mesh->texture_id = materials.at(mat_name).texture_id;

        // OpenGL setup
        glGenVertexArrays(1, &mesh->VAO);
        glGenBuffers(1, &mesh->VBO);
        
        glBindVertexArray(mesh->VAO);
        glBindBuffer(GL_ARRAY_BUFFER, mesh->VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
        
        // Vertex attributes
        GLsizei stride = 12 * sizeof(float);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(7 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(9 * sizeof(float)));
        glEnableVertexAttribArray(3);
        
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        meshes.push_back(mesh);
        total_triangles += mesh->TRIANGLE_COUNT;
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

// Function to create entity after loading
std::vector<Mesh*> create_mesh_with_obj(const std::string& name, const std::string& obj_path, glm::vec3 center, glm::vec3 rotation, glm::vec3 scale) {
    std::cout << "Loading OBJ: " << obj_path << std::endl;
    std::vector<Mesh*> meshes = loadOBJWithMTL(obj_path);

    if (meshes.empty()) {
        std::cerr << "Failed to load OBJ file: " << obj_path << std::endl;
        return {};
    }

    for (Mesh* mesh : meshes) {
        Entity new_entity; // Assuming create_entity takes these as args
        new_entity.name = name;
        new_entity.mesh = mesh;
        new_entity.position = center;
        new_entity.rotation = rotation;
        new_entity.scale = scale;
        
        entity_manager.addEntity(new_entity);
    }

    entity_count++;
    return meshes;
}

// ============================================================================
// MAIN FUNCTION
// ============================================================================

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
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    
    // Anti-aliaising
    glfwWindowHint(GLFW_SAMPLES, 4);
    
    GLFWwindow* window = glfwCreateWindow(800, 600, "C++ OpenGL 3D Engine", NULL, NULL);
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
    global_camera = create_camera(800.0f / 600.0f);
    camera_update_vectors(&global_camera);
    
    // Pixel rendering configurations
    
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
    
    create_light_source(glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), 1.0f);
        
    // CREATE TEST CUBES //
    
    Mesh* cube1 = createTestCube();
    Mesh* cube2 = createTestCube();
    Mesh* cube3 = createTestCube();
    
    Entity entity1 = create_entity("cube1", cube1, glm::vec3(-3, 0, -8), glm::vec3(0, 0, 0), glm::vec3(1, 1, 1));
    Entity entity2 = create_entity("cube2", cube2, glm::vec3(0, 0, -8), glm::vec3(0, 45, 0), glm::vec3(1, 1, 1));
    Entity entity3 = create_entity("cube3", cube3, glm::vec3(3, 0, -8), glm::vec3(0, 0, 0), glm::vec3(1, 1, 1));
    
    entity_manager.addEntity(entity1);
    entity_manager.addEntity(entity2);
    entity_manager.addEntity(entity3);
    
    printf("Total triangles: %d\n", total_triangles);
    printf("Active entities: %zu\n", entity_manager.size());
      
    // ============================================================================
    // MAIN LOOP
    // ============================================================================
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        updateFPS(window);
        
        if (!paused) {
            float camera_speed = 0.05f;
            if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
                camera_speed *= 2.0f;
            }
            
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
                global_camera.position += camera_speed * global_camera.front;
            }
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
                global_camera.position -= camera_speed * global_camera.front;
            }
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
                global_camera.position -= camera_speed * global_camera.right;
            }
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                global_camera.position += camera_speed * global_camera.right;
            }
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
                global_camera.position.y += camera_speed;
            }
            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
                global_camera.position.y -= camera_speed;
            }
            
            // Update matrices
            view = camera_get_view_matrix(&global_camera);
            projection = camera_get_projection(&global_camera);
            
            // Update lights
            lights[0].position = global_camera.position;
            
            // Rotate middle cube
            entity_manager.updateEntity("cube2", VEC3_NO_CHANGE, glm::vec3(update_count, update_count * 0.5f, 0), VEC3_NO_CHANGE);
            update_count += 1.0f;
        }
        
        // Clear background
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Render all entities
        for (size_t i = 0; i < entity_manager.size(); i++) {
            Entity* entity = entity_manager.getEntityAt(i);
            if (entity && entity->active) {
                renderer->drawMesh(entity, entity->mesh, global_camera, lights);
            }
        }
        
        glfwSwapBuffers(window);
        
        // Handle pause/unpause
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
    static bool firstMouse = true;
    
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

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;
    
    global_camera.yaw += xoffset;
    global_camera.pitch += yoffset;

    if (global_camera.pitch > 89.0f) global_camera.pitch = 89.0f;
    if (global_camera.pitch < -89.0f) global_camera.pitch = -89.0f;
    
    camera_update_vectors(&global_camera);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    (void)window;
    glViewport(0, 0, width, height);
    if (height > 0) {
        global_camera.aspect_ratio = (float)width / (float)height;
    }
}

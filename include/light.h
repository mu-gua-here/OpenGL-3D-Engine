#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <string>
#include <vector>
#include <memory>
#include <cmath>

// Forward declarations
class Mesh;
class EntityManager;
extern EntityManager entity_manager;

#define MAX_LIGHTS 8

typedef enum {
    DIR_LIGHT = 0,
    POINT_LIGHT = 1,
    SPOT_LIGHT = 2,
} LightType;

typedef struct {
    glm::vec3 position;
    glm::vec3 color;
    int intensity;
    
    glm::vec3 direction;
    float inner_cutoff_cos;
    float outer_cutoff_cos;
    int type;
    
    std::string entity_name;
} Light;

extern std::vector<Light> lights;
extern unsigned int SHADOW_WIDTH;
extern unsigned int SHADOW_HEIGHT;

// Forward declarations for functions that will be defined in main.cpp
// These are called by light functions
void createEntity(std::string name, std::vector<std::unique_ptr<Mesh>>&& meshes, glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, std::vector<int> cull_mode);

// Light system functions
glm::vec3 convertVecToEuler(glm::vec3 direction, glm::vec3 offset);
void createDirLight(std::string name, glm::vec3 direction, glm::vec3 color, int intensity);
void createSpotlight(std::string name, glm::vec3 position, glm::vec3 color, int intensity,
                     glm::vec3 direction, float inner_angle_deg, float outer_angle_deg,
                     std::vector<std::unique_ptr<class Mesh>>&& light_mesh, glm::vec3 scale, std::vector<int> cull_mode);
void createPointLight(std::string name, glm::vec3 position, glm::vec3 color, int intensity,
                      std::vector<std::unique_ptr<class Mesh>>&& light_mesh, glm::vec3 scale, std::vector<int> cull_mode);
void updateLight(std::string name, glm::vec3 position, glm::vec3 color, int intensity, glm::vec3 rotation);

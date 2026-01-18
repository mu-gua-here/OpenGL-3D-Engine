#include "light.h"
#include <cstdio>
#include <glm/gtx/euler_angles.hpp>

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
    light.inner_cutoff_cos = -1.0f;
    light.outer_cutoff_cos = -1.0f;
    light.type = DIR_LIGHT;
    light.entity_name = name;

    if (lights.size() >= MAX_LIGHTS) {
        printf("Warning: Exceeded maximum number of lights (%d). Additional lights will be ignored in shaders.\n", MAX_LIGHTS);
        return;
    }

    lights.push_back(light);
    printf("Created directional light '%s'\n", name.c_str());
}

void createSpotlight(std::string name, const std::vector<std::pair<float, std::vector<std::shared_ptr<Mesh>>>>& lodSpecs, glm::vec3 position, glm::vec3 color, int intensity,
                    glm::vec3 direction, float inner_angle_deg, float outer_angle_deg,
                    glm::vec3 scale, std::vector<int> cull_mode) {
    Light light;
    light.position = position;
    light.color = color;
    light.intensity = intensity;
    light.direction = glm::normalize(direction);
    light.inner_cutoff_cos = glm::cos(glm::radians(inner_angle_deg));
    light.outer_cutoff_cos = glm::cos(glm::radians(outer_angle_deg));
    light.type = SPOT_LIGHT;
    light.entity_name = name;

    if (lights.size() >= MAX_LIGHTS) {
        printf("Warning: Exceeded maximum number of lights (%d). Additional lights will be ignored in shaders.\n", MAX_LIGHTS);
        return;
    }

    lights.push_back(light);
    glm::vec3 rotation = convertVecToEuler(light.direction, glm::vec3(0.0f));

    if (!lodSpecs.empty()) createEntity(light.entity_name, lodSpecs, light.position, rotation, scale, cull_mode);
    printf("Created spotlight '%s' with cone angles %.1f-%.1f degrees\n", name.c_str(), inner_angle_deg, outer_angle_deg);
}

void createPointLight(std::string name, const std::vector<std::pair<float, std::vector<std::shared_ptr<Mesh>>>>& lodSpecs,
                      glm::vec3 position, glm::vec3 color, int intensity,
                      glm::vec3 scale, std::vector<int> cull_mode) {
    Light light;
    light.position = position;
    light.color = color;
    light.intensity = intensity;
    
    light.direction = glm::vec3(0.0f, -1.0f, 0.0f);
    light.inner_cutoff_cos = -1.0f;
    light.outer_cutoff_cos = -1.0f;
    light.type = POINT_LIGHT;
    light.entity_name = name;

    if (lights.size() >= MAX_LIGHTS) {
        printf("Warning: Exceeded maximum number of lights (%d). Additional lights will be ignored in shaders.\n", MAX_LIGHTS);
        return;
    }

    lights.push_back(light);
    if (!lodSpecs.empty()) createEntity(light.entity_name, lodSpecs, position, glm::vec3(0.0f), scale, cull_mode);
    printf("Created point light '%s' with intensity %d\n", name.c_str(), intensity);
}

void updateLight(std::string name, glm::vec3 position, glm::vec3 color, int intensity, glm::vec3 rotation) {
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
    }
}
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

typedef struct {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    float yaw;
    float pitch;
    float fov;
    float aspect_ratio;
    float near_plane;
    float far_plane;
    float speed_multiplier;
    float friction;
} Camera;

inline Camera create_camera(float aspect) {
    Camera cam;
    cam.position = glm::vec3(0, 2, 9);
    cam.velocity = glm::vec3(0, 0, 0);
    cam.front = glm::vec3(0, 0, -1);
    cam.up = glm::vec3(0, 1, 0);
    cam.right = glm::vec3(1, 0, 0);
    cam.yaw = -90.0f;
    cam.pitch = 0.0f;
    cam.fov = glm::radians(45.0f);
    cam.aspect_ratio = aspect;
    cam.near_plane = 0.1f;
    cam.far_plane = 200.0f; 
    cam.speed_multiplier = 0.5f;
    cam.friction = 0.9f;
    return cam;
}

inline void camera_update_vectors(Camera* cam) {
    cam->front.x = cos(glm::radians(cam->yaw)) * cos(glm::radians(cam->pitch));
    cam->front.y = sin(glm::radians(cam->pitch));
    cam->front.z = sin(glm::radians(cam->yaw)) * cos(glm::radians(cam->pitch));
    cam->front = glm::normalize(cam->front);

    cam->right = glm::normalize(glm::cross(cam->front, glm::vec3(0, 1, 0)));
    cam->up = glm::normalize(glm::cross(cam->right, cam->front));
}

inline glm::mat4 camera_get_projection(Camera* cam) {
    return glm::perspective(cam->fov, cam->aspect_ratio, cam->near_plane, cam->far_plane);
}

inline glm::mat4 camera_get_view_matrix(Camera* cam) {
    return glm::lookAt(cam->position, cam->position + cam->front, cam->up);
}

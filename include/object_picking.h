#pragma once

#include <glm/glm.hpp>
#include <vector>

#include "camera.h"
#include "entity_manager.h"
#include "physics.h"

struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
};

Ray generateRayFromMouse(double mouseX, double mouseY, const Camera& camera, const glm::mat4& projection, int screenWidth, int screenHeight);

bool raySphereIntersection(const Ray& ray, const glm::vec3& center, float radius, float& t);

bool rayAABBIntersection(const Ray& ray, const glm::vec3& min, const glm::vec3& max, float& t);

bool rayPlaneIntersection(const Ray& ray, const glm::vec3& planePoint, const glm::vec3& planeNormal, float& t);

// Picking function
Entity* pickEntity(const Ray& ray, EntityManager& entityManager, const std::vector<PhysicsBody>& physicsBodies);
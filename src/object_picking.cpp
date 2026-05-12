#include <limits>

#include "camera.h"
#include "object_picking.h"
#include "entity_manager.h"
#include "physics.h"

Ray generateRayFromMouse(double mouseX, double mouseY, const Camera& camera, const glm::mat4& projection, int screenWidth, int screenHeight) {
    // Convert mouse to normalized device coordinates (-1 to 1)
    float x = (2.0f * mouseX) / screenWidth - 1.0f;
    float y = 1.0f - (2.0f * mouseY) / screenHeight;  // Flip Y

    // Clip space
    glm::vec4 clipCoords(x, y, -1.0f, 1.0f);

    // Eye space
    glm::mat4 invProj = glm::inverse(projection);
    glm::vec4 eyeCoords = invProj * clipCoords;
    eyeCoords = glm::vec4(eyeCoords.x, eyeCoords.y, -1.0f, 0.0f);  // Forward direction

    // World space
    glm::mat4 view = glm::lookAt(camera.position, camera.position + camera.front, camera.up);
    glm::mat4 invView = glm::inverse(view);
    glm::vec4 worldCoords = invView * eyeCoords;
    glm::vec3 direction = glm::normalize(glm::vec3(worldCoords));

    return {camera.position, direction};
}

bool raySphereIntersection(const Ray& ray, const glm::vec3& center, float radius, float& t) {
    glm::vec3 oc = ray.origin - center;
    float a = glm::dot(ray.direction, ray.direction);
    float b = 2.0f * glm::dot(oc, ray.direction);
    float c = glm::dot(oc, oc) - radius * radius;
    float discriminant = b * b - 4 * a * c;
    if (discriminant < 0) return false;
    t = (-b - sqrt(discriminant)) / (2.0f * a);
    return t > 0;
}

bool rayAABBIntersection(const Ray& ray, const glm::vec3& min, const glm::vec3& max, float& t) {
    float tmin = (min.x - ray.origin.x) / ray.direction.x;
    float tmax = (max.x - ray.origin.x) / ray.direction.x;
    if (tmin > tmax) std::swap(tmin, tmax);

    float tymin = (min.y - ray.origin.y) / ray.direction.y;
    float tymax = (max.y - ray.origin.y) / ray.direction.y;
    if (tymin > tymax) std::swap(tymin, tymax);

    if ((tmin > tymax) || (tymin > tmax)) return false;
    if (tymin > tmin) tmin = tymin;
    if (tymax < tmax) tmax = tymax;

    float tzmin = (min.z - ray.origin.z) / ray.direction.z;
    float tzmax = (max.z - ray.origin.z) / ray.direction.z;
    if (tzmin > tzmax) std::swap(tzmin, tzmax);

    if ((tmin > tzmax) || (tzmin > tmax)) return false;
    if (tzmin > tmin) tmin = tzmin;
    if (tzmax < tmax) tmax = tzmax;

    t = tmin;
    return t > 0;
}

bool rayPlaneIntersection(const Ray& ray, const glm::vec3& planePoint, const glm::vec3& planeNormal, float& t) {
    float denom = glm::dot(planeNormal, ray.direction);
    if (abs(denom) > 1e-6) {
        glm::vec3 p0l0 = planePoint - ray.origin;
        t = glm::dot(p0l0, planeNormal) / denom;
        return t >= 0;
    }
    return false;
}

// Picking function
Entity* pickEntity(const Ray& ray, EntityManager& entityManager, const std::vector<PhysicsBody>& physicsBodies) {
    Entity* closestEntity = nullptr;
    float closestT = std::numeric_limits<float>::max();

    for (size_t i = 0; i < entityManager.size(); ++i) {
        Entity* entity = entityManager.getEntityAt(i);
        if (!entity || !entity->active) continue;

        // Check if entity has a physics body
        if (entity->physics_body_index >= 0 && entity->physics_body_index < (int)physicsBodies.size()) {
            const PhysicsBody& body = physicsBodies[entity->physics_body_index];
            if (!body.colliders.empty()) {
                const Collider& collider = body.colliders[0];

                glm::vec3 worldCenter = entity->position + collider.offset;
                float t = 0.0f;
                bool hit = false;

                if (collider.type == ColliderType::Sphere) {
                    hit = raySphereIntersection(ray, worldCenter, collider.radius, t);
                } else if (collider.type == ColliderType::AABB) {
                    glm::vec3 min = worldCenter - collider.halfExtents;
                    glm::vec3 max = worldCenter + collider.halfExtents;
                    hit = rayAABBIntersection(ray, min, max, t);
                }

                if (hit && t < closestT) {
                    closestT = t;
                    closestEntity = entity;
                }
            }
        } else {
            // Fallback: use a simple bounding sphere based on scale
            float radius = glm::length(entity->scale) * 0.5f;  // Rough approximation
            float t = 0.0f;
            if (raySphereIntersection(ray, entity->position, radius, t) && t < closestT) {
                closestT = t;
                closestEntity = entity;
            }
        }
    }

    return closestEntity;
}
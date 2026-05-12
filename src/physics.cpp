#include "physics.h"
#include "entity_manager.h"
#include <cmath>

extern int selectedEntityIndex;

PhysicsWorld physics_world;

PhysicsWorld::PhysicsWorld() : gravity(0.0f, -9.81f, 0.0f) {}

int PhysicsWorld::addBody(PhysicsBody body) {
    bodies.push_back(body);
    return bodies.size() - 1;
}

PhysicsBody* PhysicsWorld::getBody(int index) {
    if (index >= 0 && index < (int)bodies.size()) {
        return &bodies[index];
    }
    return nullptr;
}

void PhysicsWorld::applyForce(int bodyIndex, glm::vec3 force) {
    if (bodyIndex >= 0 && bodyIndex < (int)bodies.size()) {
        bodies[bodyIndex].force += force;
    }
}

void PhysicsWorld::step(float dt) {
    // Apply forces (gravity + accumulated forces)
    for (auto& body : bodies) {
        if (body.isStatic) continue;  // Static bodies don't move
        if (body.entityIndex == selectedEntityIndex && selectedEntityIndex >= 0) {
            // Don't apply physics to currently dragged entity
            body.force = glm::vec3(0.0f);
            continue;
        }
        
        // Add gravity
        if (body.affectedByGravity) {
            body.force += gravity * body.mass;
        }
        
        // Calculate acceleration: a = F / m
        body.acceleration = body.force * body.inverseMass;
        
        // Integrate velocity (semi-implicit Euler)
        body.velocity += body.acceleration * dt;
        
        // Apply damping (friction/air resistance)
        body.velocity *= (1.0f - body.linearDamping * dt);
        
        // Integrate position
        glm::vec3 newPos = body.velocity * dt;
        
        // Update entity position
        Entity* entity = entity_manager.getEntityAt(body.entityIndex);
        if (entity) {
            entity->position += newPos;
        }
        
        // Clear forces for next frame
        body.force = glm::vec3(0.0f);
    }
    
    // Collision detection and response
    for (auto& body : bodies) {
        if (body.isStatic) continue;
        
        Entity* entity = entity_manager.getEntityAt(body.entityIndex);
        if (!entity) continue;
        
        // Ground collision - account for collider
        float ground_y = 0.5f;
        if (!body.colliders.empty()) {
            const Collider& collider = body.colliders[0];  // Use first collider
            if (collider.type == ColliderType::Sphere) {
                float bottom_y = entity->position.y + collider.offset.y - collider.radius;
                if (bottom_y < ground_y) {
                    // Correct position
                    entity->position.y = ground_y - collider.offset.y + collider.radius;
                    // Bounce: reverse and dampen vertical velocity
                    body.velocity.y = -body.velocity.y * body.restitution;
                }
            } else if (collider.type == ColliderType::AABB) {
                // For AABB, check bottom face
                float bottom_y = entity->position.y + collider.offset.y - collider.halfExtents.y;
                if (bottom_y < ground_y) {
                    entity->position.y = ground_y - collider.offset.y + collider.halfExtents.y;
                    body.velocity.y = -body.velocity.y * body.restitution;
                }
            }
        } else {
            // No collider, treat as point
            if (entity->position.y < ground_y) {
                entity->position.y = ground_y;
                body.velocity.y = -body.velocity.y * body.restitution;
            }
        }
    }
}

void createPhysicsEntity(const std::string& entityName, float mass, float linearDamping, 
                         float restitution, bool isStatic, bool affectedByGravity, 
                         const std::vector<Collider>& colliders) {
    
    int entityIndex = entity_manager.findEntity(entityName).value();
    
    PhysicsBody body;
    body.entityIndex = entityIndex;
    body.mass = mass;
    body.inverseMass = (mass > 0.0f) ? 1.0f / mass : 0.0f;
    body.linearDamping = linearDamping;
    body.restitution = restitution;
    body.isStatic = isStatic;
    body.affectedByGravity = affectedByGravity;
    body.velocity = glm::vec3(0.0f);
    body.colliders = colliders;

    // Add to physics world
    int physics_idx = physics_world.addBody(body);

    Entity* entity = entity_manager.getEntityAt(body.entityIndex);
    if (entity) {
        entity->physics_body_index = physics_idx;
    }
}
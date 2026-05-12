#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>

// Collider types
enum class ColliderType {
    Sphere,
    AABB,      // Axis-aligned bounding box
    Plane
};

// Defines a collision shape attached to a physics body
struct Collider {
    ColliderType type;
    glm::vec3 offset;           // Local offset from body center
    float radius;         // For sphere
    glm::vec3 halfExtents; // For AABB
    glm::vec3 normal;      // For plane

    // Constructor
    Collider(ColliderType t, glm::vec3 o, float r, glm::vec3 n = glm::vec3(0.0f))
        : type(t), offset(o), radius(r), normal(n) {}
};

// A physics body attached to an entity
struct PhysicsBody {
    int entityIndex;            // Points back to which entity this belongs to
    
    // Dynamics
    glm::vec3 velocity = glm::vec3(0.0f);
    glm::vec3 acceleration;
    glm::vec3 force;            // Force accumulator
    
    // Mass properties
    float mass;
    float inverseMass;          // 1.0 / mass (or 0 if static)
    
    // Damping
    float linearDamping;        // Friction/air resistance (0-1)
    
    // Collision response
    float restitution;          // Bounce (0 = no bounce, 1 = perfect bounce)
    
    // State
    bool isStatic;              // If true, doesn't move (infinite mass)
    bool affectedByGravity;
    
    // Colliders
    std::vector<Collider> colliders;
};

// Main physics world/system
class PhysicsWorld {
private:
    std::vector<PhysicsBody> bodies;
    glm::vec3 gravity;
    
public:
    PhysicsWorld();
    
    // Add a physics body to the world
    int addBody(PhysicsBody body);
    
    // Main physics step (called once per fixed timestep)
    void step(float dt);
    
    // Set gravity
    void setGravity(glm::vec3 g) { gravity = g; }
    
    // Get bodies (for picking)
    const std::vector<PhysicsBody>& getBodies() const { return bodies; }
    
    // Get a body
    PhysicsBody* getBody(int index);
    
    // Apply a force to a body
    void applyForce(int bodyIndex, glm::vec3 force);
    
    // Get all bodies
    std::vector<PhysicsBody>& getBodies() { return bodies; }
};

void createPhysicsEntity(const std::string& entityName, float mass, float linearDamping, float restitution, bool isStatic, bool affectedByGravity, const std::vector<Collider>& colliders);

extern PhysicsWorld physics_world;
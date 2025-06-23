//
//  main.mm
//  OpenGL Test
//
//  Created by mu_gua_here on 2025/6/20.
//

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>
#include <OpenGL/gl3.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <cmath>

// --- Function Prototypes ---
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// Runtime
bool paused = false;

// Engine
int entity_count = 0;

// ============================================================================
// MATH LIBRARY
// ============================================================================

typedef struct {
    float x, y, z;
} Vec3;

typedef struct {
    float m[16]; // 4x4 matrix stored column-major
} Mat4;

// Basic vector operations
Vec3 vec3_add(Vec3 a, Vec3 b) {
    return (Vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 vec3_scale(Vec3 v, float s) {
    return (Vec3){v.x * s, v.y * s, v.z * s};
}

Vec3 vec3_sub(Vec3 a, Vec3 b) {
    return (Vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

float vec3_length(Vec3 v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

Vec3 vec3_normalize(Vec3 v) {
    float len = vec3_length(v);
    if (len == 0.0f) return (Vec3){0,0,0};
    return vec3_scale(v, 1.0f / len);
}

Vec3 vec3_cross(Vec3 a, Vec3 b) {
    return (Vec3){a.y * b.z - a.z * b.y,
                  a.z * b.x - a.x * b.z,
                  a.x * b.y - a.y * b.x};
}

float vec3_dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Matrix multiplication
Mat4 mat4_multiply(Mat4 a, Mat4 b) {
    Mat4 result = {0};
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            for (int k = 0; k < 4; k++) {
                result.m[i + j * 4] += a.m[i + k * 4] * b.m[k + j * 4];
            }
        }
    }
    return result;
}

// Identity matrix
Mat4 mat4_identity() {
    Mat4 m = {0};
    m.m[0] = m.m[5] = m.m[10] = m.m[15] = 1.0f;
    return m;
}

// Perspective projection matrix
Mat4 mat4_perspective(float fov, float aspect, float near, float far) {
    Mat4 m = {0};
    float f = 1.0f / tanf(fov * 0.5f);
    m.m[0] = f / aspect;
    m.m[5] = f;
    m.m[10] = (far + near) / (near - far);
    m.m[11] = -1.0f;
    m.m[14] = (2.0f * far * near) / (near - far);
    return m;
}

// Translation matrix
Mat4 mat4_translate(Vec3 pos) {
    Mat4 m = mat4_identity();
    m.m[12] = pos.x;
    m.m[13] = pos.y;
    m.m[14] = pos.z;
    return m;
}

// Rotation matrix around X axis (pitch)
Mat4 mat4_rotate_x(float angle) {
    Mat4 m = mat4_identity();
    float c = cosf(angle);
    float s = sinf(angle);
    m.m[5] = c;    m.m[6] = -s;  // Fixed sign
    m.m[9] = s;    m.m[10] = c;  // Fixed sign
    return m;
}

// Rotation matrix around Y axis (yaw)
Mat4 mat4_rotate_y(float angle) {
    Mat4 m = mat4_identity();
    float c = cosf(angle);
    float s = sinf(angle);
    m.m[0] = c;    m.m[2] = -s;  // Fixed sign
    m.m[8] = s;    m.m[10] = c;  // Fixed sign
    return m;
}

// Rotation matrix around Z axis (roll)
Mat4 mat4_rotate_z(float angle) {
    Mat4 m = mat4_identity();
    float c = cosf(angle);
    float s = sinf(angle);
    m.m[0] = c;    m.m[1] = -s;  // Fixed sign
    m.m[4] = s;    m.m[5] = c;   // Fixed sign
    return m;
}

// Create scaling matrix
Mat4 mat4_scale(Vec3 scale) {
    Mat4 m = mat4_identity();
    m.m[0] = scale.x;
    m.m[5] = scale.y;
    m.m[10] = scale.z;
    return m;
}

// LookAt matrix
Mat4 mat4_look_at(Vec3 eye, Vec3 center, Vec3 up) {
    Vec3 f = vec3_normalize(vec3_sub(center, eye)); // Forward vector
    Vec3 s = vec3_normalize(vec3_cross(f, up));      // Side vector
    Vec3 u = vec3_cross(s, f);                      // Up vector (re-orthogonalized)

    Mat4 result = mat4_identity();
    result.m[0] = s.x;
    result.m[4] = s.y;
    result.m[8] = s.z;
    result.m[1] = u.x;
    result.m[5] = u.y;
    result.m[9] = u.z;
    result.m[2] = -f.x; // Negative Z because OpenGL is right-handed
    result.m[6] = -f.y;
    result.m[10] = -f.z;
    result.m[12] = -vec3_dot(s, eye);
    result.m[13] = -vec3_dot(u, eye);
    result.m[14] = vec3_dot(f, eye);  // Fixed sign
    return result;
}

// ============================================================================
// MESH SYSTEM (3D Object representation)
// ============================================================================

typedef struct {
    float* vertices;
    unsigned int* indices;
    int vertex_count;
    int index_count;
    unsigned int VAO, VBO, EBO;
} Mesh;

// Create a cube mesh
Mesh create_cube() {
    static float cube_vertices[] = {
        // positions (3 floats)
        // Front face
        -0.5f, -0.5f,  0.5f, // 0: Bottom left
         0.5f, -0.5f,  0.5f, // 1: Bottom right
         0.5f,  0.5f,  0.5f, // 2: Top right
        -0.5f,  0.5f,  0.5f, // 3: Top left
        // Back face
        -0.5f, -0.5f, -0.5f, // 4: Bottom left
         0.5f, -0.5f, -0.5f, // 5: Bottom right
         0.5f,  0.5f, -0.5f, // 6: Top right
        -0.5f,  0.5f, -0.5f  // 7: Top left
    };

    static unsigned int cube_indices[] = {
        // Front face
        0, 1, 2,  2, 3, 0,
        // Back face
        4, 5, 6,  6, 7, 4,
        // Left face
        7, 3, 0,  0, 4, 7,
        // Right face
        1, 5, 6,  6, 2, 1,
        // Top face
        3, 2, 6,  6, 7, 3,
        // Bottom face
        0, 1, 5,  5, 4, 0
    };

    Mesh mesh;
    mesh.vertices = cube_vertices;
    mesh.indices = cube_indices;
    mesh.vertex_count = 8;
    mesh.index_count = 36;

    // Buffers
    glGenVertexArrays(1, &mesh.VAO);
    glGenBuffers(1, &mesh.VBO);
    glGenBuffers(1, &mesh.EBO);

    glBindVertexArray(mesh.VAO);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_indices), cube_indices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0); // Unbind VAO

    return mesh;
}

// ============================================================================
// ENTITY SYSTEM (Game objects in 3D space)
// ============================================================================

typedef struct {
    Vec3 position;
    Vec3 rotation; // Keep for object rotation
    Vec3 scale;
    Mesh* mesh;
    int active;
} Entity;

// Create a new entity
Entity create_entity(Mesh* mesh, Vec3 pos) {
    Entity entity;
    entity.position = pos;
    entity.rotation = (Vec3){0, 0, 0};
    entity.scale = (Vec3){1, 1, 1};
    entity.mesh = mesh;
    entity.active = 1;
    entity_count += 1;
    return entity;
}

// ============================================================================
// CAMERA SYSTEM
// ============================================================================

typedef struct {
    Vec3 position;
    Vec3 front; // Direction camera is looking
    Vec3 up;    // Up direction (usually fixed at (0,1,0))
    Vec3 right; // Right direction

    float yaw;   // Y-axis rotation (left/right)
    float pitch; // X-axis rotation (up/down)

    float fov;
    float aspect_ratio;
    float near_plane;
    float far_plane;
} Camera;

Camera global_camera;

Camera create_camera(float aspect) {
    Camera cam;
    cam.position = (Vec3){0, 0, 5};
    cam.front = (Vec3){0, 0, -1}; // Initially look towards negative Z
    cam.up = (Vec3){0, 1, 0};
    cam.right = (Vec3){1, 0, 0}; // Will be calculated

    cam.yaw = -90.0f;
    cam.pitch = 0.0f;

    cam.fov = 3.14159f / 4.0f; // 45 degrees in radians
    cam.aspect_ratio = aspect;
    cam.near_plane = 0.1f;
    cam.far_plane = 100.0f;

    return cam;
}

// Function to update camera's front, right, and up vectors based on yaw and pitch
void camera_update_vectors(Camera* cam) {
    // Calculate new front vector
    cam->front.x = cosf(cam->yaw * (M_PI / 180.0f)) * cosf(cam->pitch * (M_PI / 180.0f));
    cam->front.y = sinf(cam->pitch * (M_PI / 180.0f));
    cam->front.z = sinf(cam->yaw * (M_PI / 180.0f)) * cosf(cam->pitch * (M_PI / 180.0f));
    cam->front = vec3_normalize(cam->front);

    // Calculate right and up vector
    // OpenGL's world up is (0,1,0)
    cam->right = vec3_normalize(vec3_cross(cam->front, (Vec3){0, 1, 0}));
    cam->up = vec3_normalize(vec3_cross(cam->right, cam->front));
}

Mat4 camera_get_projection(Camera* cam) {
    return mat4_perspective(cam->fov, cam->aspect_ratio, cam->near_plane, cam->far_plane);
}

// Calculates the view matrix using the LookAt function
Mat4 camera_get_view_matrix(Camera* cam) {
    Vec3 target = vec3_add(cam->position, cam->front);
    return mat4_look_at(cam->position, target, cam->up);
}

// ============================================================================
// SHADER SYSTEM
// ============================================================================

typedef struct {
    unsigned int program;
    int mvp_location; // Model-View-Projection matrix uniform location
} Shader;

const char* vertex_shader_3d = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "uniform mat4 uMVP;\n"
    "void main() {\n"
    "   gl_Position = uMVP * vec4(aPos, 1.0);\n"
    "}\0";

const char* fragment_shader_3d = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "   FragColor = vec4(0.8f, 0.4f, 0.2f, 1.0f);\n"
    "}\0";

Shader create_shader() {
    // Compile vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertex_shader_3d, NULL);
    glCompileShader(vertexShader);
    
    // Check vertex shader compilation
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        printf("Vertex shader compilation failed: %s\n", infoLog);
    }

    // Compile fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragment_shader_3d, NULL);
    glCompileShader(fragmentShader);
    
    // Check fragment shader compilation
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        printf("Fragment shader compilation failed: %s\n", infoLog);
    }

    // Link shader program
    unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    // Check program linking
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        printf("Shader program linking failed: %s\n", infoLog);
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    Shader shader;
    shader.program = program;
    shader.mvp_location = glGetUniformLocation(program, "uMVP");

    return shader;
}

// ============================================================================
// RENDERER SYSTEM
// ============================================================================

typedef struct {
    Shader shader;
} Renderer;

void renderer_init(Renderer* renderer, float aspect) {
    renderer->shader = create_shader();
    global_camera = create_camera(aspect); // Initialize global camera here

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
}

void renderer_draw_entity(Renderer* renderer, Entity* entity) {
    if (!entity->active) return;

    // Calculate transformation matrices using proper helper functions
    Mat4 scale_matrix = mat4_scale(entity->scale);
    
    // Apply rotations in ZYX order (roll, pitch, yaw) for typical Euler angles
    Mat4 rotation_x = mat4_rotate_x(entity->rotation.x);
    Mat4 rotation_y = mat4_rotate_y(entity->rotation.y);
    Mat4 rotation_z = mat4_rotate_z(entity->rotation.z);
    Mat4 rotation = mat4_multiply(rotation_z, mat4_multiply(rotation_y, rotation_x));

    Mat4 translation = mat4_translate(entity->position);

    // Combine transformations: Translation * Rotation * Scale
    Mat4 model = mat4_multiply(translation, mat4_multiply(rotation, scale_matrix));

    // Get view and projection from the GLOBAL camera
    Mat4 view = camera_get_view_matrix(&global_camera);
    Mat4 projection = camera_get_projection(&global_camera);

    // Calculate MVP: Projection * View * Model
    Mat4 mvp = mat4_multiply(projection, mat4_multiply(view, model));

    // Use shader and set uniforms
    glUseProgram(renderer->shader.program);
    glUniformMatrix4fv(renderer->shader.mvp_location, 1, GL_FALSE, mvp.m);

    // Draw the mesh
    glBindVertexArray(entity->mesh->VAO);
    glDrawElements(GL_TRIANGLES, entity->mesh->index_count, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0); // Unbind VAO after drawing
}

// ============================================================================
// MAIN ENGINE LOOP
// ============================================================================

int main() {
    // Initialize GLFW and create window
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(800, 600, "C++ OpenGL 3D Engine", NULL, NULL);
    if (!window) {
        printf("Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Set callbacks
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // Initialize renderer (which also initializes the global_camera)
    Renderer renderer;
    renderer_init(&renderer, 800.0f / 600.0f);
    // IMPORTANT: Call camera_update_vectors once after initialization to set initial front/right/up
    camera_update_vectors(&global_camera);

    // Create some 3D objects (smaller cubes, spread further apart)
    Mesh cube_mesh = create_cube();

    Entity entities[3];
    entities[0] = create_entity(&cube_mesh, (Vec3){-3, 0, -5});
    entities[1] = create_entity(&cube_mesh, (Vec3){0, 0, -5});
    entities[2] = create_entity(&cube_mesh, (Vec3){3, 0, -5});

    printf("Basic 3D Engine initialized!\n");
    printf("3D Objects: %d cubes\n", entity_count);
    printf("Controls: WASD to move, mouse to look around, E/Q to move up/down, ESC to exit\n");
    
    // Main render loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents(); // This processes events and calls mouse_callback
        
        if (paused == false) {
            // Keyboard input for camera movement
            float camera_speed = 0.05f;
            if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
                camera_speed *= 2;
            }
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
                global_camera.position = vec3_add(global_camera.position, vec3_scale(global_camera.front, camera_speed));
            }
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
                global_camera.position = vec3_sub(global_camera.position, vec3_scale(global_camera.front, camera_speed));
            }
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
                global_camera.position = vec3_sub(global_camera.position, vec3_scale(global_camera.right, camera_speed));
            }
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                global_camera.position = vec3_add(global_camera.position, vec3_scale(global_camera.right, camera_speed));
            }
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
                global_camera.position.y += camera_speed;
            }
            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
                global_camera.position.y -= camera_speed;
            }
            
            // Update entities
            float time = glfwGetTime();
            entities[0].rotation.x = time * 0.5f;
            entities[1].rotation.y = time * 1.0f;
            entities[2].rotation.z = time * 1.5f;
        }

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);  // Black background
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Render all entities
        for (int i = 0; i < entity_count; i++) {
            renderer_draw_entity(&renderer, &entities[i]);
        }

        glfwSwapBuffers(window);

        // Set cursor modes
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

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwTerminate();
    return 0;
}

// ============================================================================
// USER INPUT
// ============================================================================

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    // Static variables to store previous mouse position and a flag for the first movement (center of window)
    static double lastX = 400.0;
    static double lastY = 300.0;
    static bool firstMouse = true; // Flag to handle the initial jump when pointer lock starts
    
    if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED) return;

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    // Calculate the offset from the last frame
    double xoffset = xpos - lastX;
    double yoffset = lastY - ypos; // Inverted Y-axis

    // Update lastX and lastY for the next frame
    lastX = xpos;
    lastY = ypos;

    // Apply sensitivity to mouse movement
    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    // Update camera's yaw and pitch
    global_camera.yaw += xoffset;
    global_camera.pitch += yoffset;

    // Constrain pitch to avoid flipping the camera upside down
    if (global_camera.pitch > 89.0f) {
        global_camera.pitch = 89.0f;
    }
    if (global_camera.pitch < -89.0f) {
        global_camera.pitch = -89.0f;
    }
    
    // After updating yaw/pitch, recalculate the camera's front, right, and up vectors
    camera_update_vectors(&global_camera);
}

// Callback for when the window is resized
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    // Adjust the OpenGL viewport to match the new window dimensions
    glViewport(0, 0, width, height);

    // Update camera aspect ratio
    if (height > 0) {
        global_camera.aspect_ratio = (float)width / (float)height;
    }
}

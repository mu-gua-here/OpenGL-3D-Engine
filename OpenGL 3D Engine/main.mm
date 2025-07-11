//
//  main.mm
//  OpenGL Test
//
//  Created by Ray Hsiao Muguang on 2025/6/20.
//

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>
#include <OpenGL/gl3.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <cmath>
#include <iostream>
#include "stb_image.h"

// --- Function Prototypes ---
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// Runtime
bool paused = false;

// Engine
unsigned int entity_count = 0;

// FPS counter
double lastTime = 0.0;
unsigned int frameCount = 0;
double lastFrameTime = 0.0;

void updateFPS(GLFWwindow* window) {
    double currentTime = glfwGetTime();
    frameCount++;
    
    // Calculate frame time in milliseconds
    double frameTime = (currentTime - lastFrameTime) * 1000.0;
    lastFrameTime = currentTime;
    
    // Update FPS counter every second
    if (currentTime - lastTime >= 1.0) {
        double fps = frameCount / (currentTime - lastTime);
        
        char title[256];
        snprintf(title, sizeof(title), "C++ OpenGL 3D Engine - FPS: %.1f | Frame Time: %.2f ms", fps, frameTime);
        glfwSetWindowTitle(window, title);
                
        frameCount = 0;
        lastTime = currentTime;
    }
}

// ============================================================================
// LOAD TEXTURES
// ============================================================================

GLuint loadTexture(const char* path) {
    int width, height, channels;
    
    // Load image data
    unsigned char* data = stbi_load(path, &width, &height, &channels, 0);
    if (!data) {
        std::cerr << "Failed to load texture: " << path << std::endl;
        return 0;
    }
    
    // Generate OpenGL texture
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Upload texture data
    GLenum format;
    if (channels == 1) format = GL_RED;
    else if (channels == 3) format = GL_RGB;
    else if (channels == 4) format = GL_RGBA;
    else {
        printf("Invalid color channel format");
        format = NULL;
    }
    
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    // Clean up
    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return textureID;
}

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

// Lookat matrix
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
    int index_count;
    unsigned int VAO, VBO, EBO;
} Mesh;

// UV coordinates
typedef struct {
    float u, v;
} Vec2;

// RGBA color structure
typedef struct {
    float r, g, b, a;
} Color;

// Backface culling modes
typedef enum {
    CULL_NONE = 0,
    CULL_BACK = 1,
    CULL_FRONT = 2
} CullMode;

// Triangle mesh structure
typedef struct {
    float vertices[36]; // 3 vertices * 12 floats per vertex (pos + color + uv + normal)
    unsigned int indices[3]; // Always just 0, 1, 2 for a triangle
    unsigned int VAO, VBO, EBO;
    GLuint texture_id;
    CullMode cull_mode;
} TriangleMesh;

// Create a triangle from three positions, color, texture, UVs, and culling mode
TriangleMesh create_triangle(Vec3 pos1, Vec3 pos2, Vec3 pos3,
                            Color color,
                            GLuint texture_id,
                            Vec2 uv1, Vec2 uv2, Vec2 uv3,
                            CullMode cull_mode) {
    
    TriangleMesh triangle;
    triangle.texture_id = texture_id;
    triangle.cull_mode = cull_mode;
    
    // Calculate face normal using cross product
    Vec3 edge1 = vec3_sub(pos2, pos1);
    Vec3 edge2 = vec3_sub(pos3, pos1);
    Vec3 normal = vec3_normalize(vec3_cross(edge1, edge2));
    
    // Build vertex data array: position(3) + color(4) + uvs(2) + normals(3) = 12 floats per vertex
    float vertices[] = {
        // Vertex 1
        pos1.x, pos1.y, pos1.z,           // Position
        color.r, color.g, color.b, color.a, // Color
        uv1.u, uv1.v,                     // UV coordinates
        normal.x, normal.y, normal.z,     // Normal
        
        // Vertex 2
        pos2.x, pos2.y, pos2.z,           // Position
        color.r, color.g, color.b, color.a, // Color
        uv2.u, uv2.v,                     // UV coordinates
        normal.x, normal.y, normal.z,     // Normal
        
        // Vertex 3
        pos3.x, pos3.y, pos3.z,           // Position
        color.r, color.g, color.b, color.a, // Color
        uv3.u, uv3.v,                     // UV coordinates
        normal.x, normal.y, normal.z      // Normal
    };
    
    // Copy vertices to triangle structure
    for (int i = 0; i < 36; i++) {
        triangle.vertices[i] = vertices[i];
    }
    
    triangle.indices[0] = 0;
    triangle.indices[1] = 1;
    triangle.indices[2] = 2;
    
    // Create OpenGL buffers
    glGenVertexArrays(1, &triangle.VAO);
    glGenBuffers(1, &triangle.VBO);
    glGenBuffers(1, &triangle.EBO);
    
    glBindVertexArray(triangle.VAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, triangle.VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangle.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(triangle.indices), triangle.indices, GL_STATIC_DRAW);
    
    // Set up vertex attributes (same as your cube setup)
    // Position attribute (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Color attribute (location 1)
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Texture coordinate attribute (location 2)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(7 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    // Normal attribute (location 3)
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(9 * sizeof(float)));
    glEnableVertexAttribArray(3);
    
    glBindVertexArray(0);
    
    return triangle;
}

// Updated entity system to work with triangles
typedef struct {
    Vec3 position;
    Vec3 rotation;
    Vec3 scale;
    TriangleMesh* triangle_mesh; // Use triangle mesh instead of general mesh
    int active;
} TriangleEntity;

// Create triangle entity
TriangleEntity create_entity(TriangleMesh* triangle_mesh, Vec3 pos) {
    TriangleEntity entity;
    entity.position = pos;
    entity.rotation = (Vec3){0, 0, 0};
    entity.scale = (Vec3){1, 1, 1};
    entity.triangle_mesh = triangle_mesh;
    entity.active = 1;
    return entity;
}

// ============================================================================
// ENTITY SYSTEM (Game objects in 3D space)
// ============================================================================

typedef struct {
    Vec3 position;
    Vec3 rotation;
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
    Vec3 up;    // Up direction
    Vec3 right; // Right direction

    float yaw;   // Y-axis rotation
    float pitch; // X-axis rotation

    float fov;
    float aspect_ratio;
    float near_plane;
    float far_plane;
} Camera;

Camera global_camera;

Camera create_camera(float aspect) {
    Camera cam;
    cam.position = (Vec3){0, 0, 5};
    cam.front = (Vec3){0, 0, -1};
    cam.up = (Vec3){0, 1, 0};
    cam.right = (Vec3){1, 0, 0};

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
    int texture_location;
} Shader;

const char* vertex_shader = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec4 aColor;\n"
    "layout (location = 2) in vec2 aTexCoord;\n"
    "out vec4 vertexColor;\n"
    "out vec2 TexCoord;\n"
    "uniform mat4 uMVP;\n"
    "void main() {\n"
    "   gl_Position = uMVP * vec4(aPos, 1.0);\n"
    "   vertexColor = aColor;\n"
    "   TexCoord = aTexCoord;\n"
    "}\0";

const char* fragment_shader = "#version 330 core\n"
    "in vec4 vertexColor;\n"
    "in vec2 TexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D u_texture;\n"
    "void main() {\n"
    "FragColor = texture(u_texture, TexCoord) * vertexColor;\n"
    "}\0";

Shader create_shader() {
    // Compile vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertex_shader, NULL);
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
    glShaderSource(fragmentShader, 1, &fragment_shader, NULL);
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
    shader.texture_location = glGetUniformLocation(program, "u_texture");


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

// Render function for triangle entities
void renderer_draw_triangle(Renderer* renderer, TriangleEntity* entity) {
    if (!entity->active) return;
    
    // Set up culling based on triangle's cull mode
    switch (entity->triangle_mesh->cull_mode) {
        case CULL_NONE:
            glDisable(GL_CULL_FACE);
            break;
        case CULL_BACK:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            break;
        case CULL_FRONT:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            break;
    }
    
    // Same transformation logic as your existing renderer
    Mat4 scale_matrix = mat4_scale(entity->scale);
    Mat4 rotation_x = mat4_rotate_x(entity->rotation.x);
    Mat4 rotation_y = mat4_rotate_y(entity->rotation.y);
    Mat4 rotation_z = mat4_rotate_z(entity->rotation.z);
    Mat4 rotation = mat4_multiply(rotation_z, mat4_multiply(rotation_y, rotation_x));
    Mat4 translation = mat4_translate(entity->position);
    Mat4 model = mat4_multiply(translation, mat4_multiply(rotation, scale_matrix));
    
    Mat4 view = camera_get_view_matrix(&global_camera);
    Mat4 projection = camera_get_projection(&global_camera);
    Mat4 mvp = mat4_multiply(projection, mat4_multiply(view, model));
    
    // Use shader and set uniforms
    glUseProgram(renderer->shader.program);
    glUniformMatrix4fv(renderer->shader.mvp_location, 1, GL_FALSE, mvp.m);
    
    // Bind the triangle's specific texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, entity->triangle_mesh->texture_id);
    glUniform1i(renderer->shader.texture_location, 0);
    
    // Draw the triangle
    glBindVertexArray(entity->triangle_mesh->VAO);
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

// Cleanup function for triangle mesh
void cleanup_triangle_mesh(TriangleMesh* triangle) {
    glDeleteVertexArrays(1, &triangle->VAO);
    glDeleteBuffers(1, &triangle->VBO);
    glDeleteBuffers(1, &triangle->EBO);
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

    // Initialize renderer
    Renderer renderer;
    renderer_init(&renderer, 800.0f / 600.0f);
    camera_update_vectors(&global_camera);
    
    // LOAD ALL TEXTURES
    
    GLuint all_textures[] = {0};
    all_textures[0] = loadTexture("container.jpg");
    
    // Create some 3D objects
    
    TriangleMesh triangle_mesh = create_triangle((Vec3) {0, 0, 0}, (Vec3) {1, 0, 0}, (Vec3) {1, 1, 0}, {1, 1, 1, 1}, all_textures[0], (Vec2) {0, 0}, (Vec2) {1, 0}, (Vec2) {1, 1}, CULL_BACK);
    
    TriangleEntity entities[2];
    entities[0] = create_entity(&triangle_mesh, (Vec3){0, 0, 0});
    entities[1] = create_entity(&triangle_mesh, (Vec3){3, 0, 0});
    
    printf("3D Engine initialized!\n");
    printf("3D Objects: %d cubes\n", entity_count);
    printf("Controls: WASD to move, mouse to look around, E/Q to move up/down, ESC to exit\n");
    
    // Main render loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents(); // This processes events and calls mouse_callback
        updateFPS(window);
        
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
            for (unsigned int i = 0; i < entity_count; i++) {
                entities[i].rotation.x = time * 0.5f;
            }
        }

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Render all entities
        for (int i = 0; i < 2; i++) {
            renderer_draw_triangle(&renderer, &entities[i]);
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

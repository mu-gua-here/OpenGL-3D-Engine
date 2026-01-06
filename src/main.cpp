//
//  main.cpp
//  OpenGL 3D Engine
//
//  Created by mu-gua-here on 2025/9/15.
//

/*
 
 CC ATTRIBUTION
 3D MODELS:
 Realistic tree model: "Tree02" (https://free3d.com/3d-model/tree02-35663.html) by rezashams313 is licensed under Creative Commons Attribution-NonCommercial (http://creativecommons.org/licenses/by-nc/4.0/).
 Road track model: "Road Modular" (https://skfb.ly/pxYZw) by golukumar is licensed under Creative Commons Attribution (http://creativecommons.org/licenses/by/4.0/).
 Residential buildings model: "Low Poly Residential Buildings Pack" (https://free3d.com/3d-model/array-house-example-3033.html) by 3dhaupt is licensed under Creative Commons Attribution-NonCommercial (http://creativecommons.org/licenses/by-nc/4.0/).
 Lamppost model: "Street Lantern" (https://skfb.ly/oMIXI) by Samuel F. Johanns (Oneironauticus) is licensed under Creative Commons Attribution (http://creativecommons.org/licenses/by/4.0/). 
 
 TEXTURES:
 Checkered grass texture: "Green grass field pattern background for soccer and football. | Premium Photo" (https://ar.pinterest.com/pin/797629784037209282/) by Freepik is licensed under Creative Commons Attribution (http://creativecommons.org/licenses/by/4.0/).

 IMPLEMENTATION LIST
 1. Physics (maybe)
 2. Optimizations
    a. Batching
    b. Instancing
    c. Frustum culling
    d. LOD
 3. Complete PBR implementation (esp. IBL)
 4. Increase view distance for shadows
 
 BUGS & IMPROVEMENTS LIST
 1. Fix ImGui window mouse interaction
 2. Fix lamppost mesh

 NOTES FOR OTHER DEVELOPERS
 1. If you try to export the textures here elsewhere it might look strange because I'd flipped the textures for them to work in OpenGL

 */

// OpenGL-related
#include "glad/glad.h"
#include <GLFW/glfw3.h>

// ImGui
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// STB (image loading)
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// GLM library
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>

// Emscripten
#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
    #include <emscripten/html5.h>
#endif

// C++ extensions
#include <iostream>
#include <vector>

// Function prototypes
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);

// ============================================================================
// FILE INCLUDES
// ============================================================================

#include "asset_loader.h"
#include "camera.h"
#include "color.h"
#include "entity_manager.h"
#include "filesystem.h"
#include "light.h"
#include "material.h"
#include "mesh_loader.h"
#include "mesh.h"
#include "renderer.h"
#include "shader.h"
#include "shadowmap.h"
#include "skybox.h"

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// System
int WINDOW_WIDTH = 800;
int WINDOW_HEIGHT = 600;

// Runtime
bool firstMouse = true;
double fps;
bool paused = false;
float update_count = 0;
float frame_time = 1.0f;
float tick_speed = 1.0f;
bool fullscreen = false;
bool initialization_complete = false;  // Loading state tracker

// Performance queries
GLuint shadowQueries[2] = {0, 0};
GLuint mainQueries[2] = {0, 0};
GLuint skyboxQueries[2] = {0, 0};
int queryIndex = 0;
int prevIndex = 0;
double shadowTime = 0.0;
double mainTime = 0.0;
double skyboxTime = 0.0;

// Player/camera
float cam_speed_multiplier = 0.005f;
glm::vec3 global_camera_vel = {0, 0, 0};
float friction = 0.9f;
glm::mat4 view;
glm::mat4 projection;
Camera global_camera;

// Scene stats
unsigned int total_triangles = 0;
unsigned int entity_count = 0;

// Shadow mapping (defined in shadowmap.cpp)
extern GLuint shadowMapFBO;
extern GLuint shadowMapTexture;
extern unsigned int SHADOW_WIDTH;
extern unsigned int SHADOW_HEIGHT;
glm::mat4 lightSpaceMatrix;

// Game settings
#define MAX_LIGHTS 8
float mouse_sensitivity = 0.1f;

// Skybox
unsigned int skyboxID;
Skybox* g_skybox = nullptr;  // Global pointer to skybox

GLuint default_texture_id = 0;

const glm::vec3 VEC3_NO_CHANGE = glm::vec3(NAN, NAN, NAN);

// ============================================================================
// EMSCRIPTEN CONTEXT
// ============================================================================

// Forward declaration (will be defined later)
class Renderer;

// Struct
struct AppContext {
    GLFWwindow* window = nullptr;
    std::unique_ptr<Renderer> renderer;
    bool should_close = false;
};

AppContext* g_app_context = nullptr;

// ============================================================================
// FPS COUNTER
// ============================================================================

void updateFPS(GLFWwindow* window) {
    static double lastTime = glfwGetTime();
    static unsigned int frameCount = 0;
    static double fpsTimer = 0.0;
    
    double currentTime = glfwGetTime();
    frame_time = currentTime - lastTime;
    lastTime = currentTime;
    tick_speed = frame_time / 0.0083f;
    
    frameCount++;
    fpsTimer += frame_time;
    
    if (fpsTimer >= 1.0) {
        // Update fps counter
        fps = frameCount / fpsTimer;
        char title[256];
        snprintf(title, sizeof(title), "OpenGL 3D Engine by @mu-gua-here");
        glfwSetWindowTitle(window, title);
        frameCount = 0;
        fpsTimer = 0.0;

        // Update performance query results - only on native platforms
        #ifndef __EMSCRIPTEN__
            GLuint64 shadowTimeNS = 0;
            glGetQueryObjectui64v(shadowQueries[prevIndex], GL_QUERY_RESULT, &shadowTimeNS);
            shadowTime = shadowTimeNS / 1000000.0;

            GLuint64 skyboxTimeNS = 0;
            glGetQueryObjectui64v(skyboxQueries[prevIndex], GL_QUERY_RESULT, &skyboxTimeNS);
            skyboxTime = skyboxTimeNS / 1000000.0;

            GLuint64 mainTimeNS = 0;
            glGetQueryObjectui64v(mainQueries[prevIndex], GL_QUERY_RESULT, &mainTimeNS);
            mainTime = mainTimeNS / 1000000.0;
        #endif
    }
}

// ============================================================================
// TEXTURE LOADING
// ============================================================================

GLuint createDefaultTexture() {
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    
    const unsigned char white_pixel[4] = {255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white_pixel);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture_id;
}

class Mesh;
Material createMaterialFromAssimp(std::string modelPath, aiMaterial* material, const aiScene* scene);
std::vector<std::unique_ptr<Mesh>> loadMesh(const std::string& filepath);

// ============================================================================
// MAIN LOOP CALLBACK
// ============================================================================

void emscripten_main_loop_callback() {
    if (!g_app_context || g_app_context->should_close) {
        #ifdef __EMSCRIPTEN__
            emscripten_cancel_main_loop();
        #endif
        return;
    }
    
    GLFWwindow* window = g_app_context->window;
    Renderer* renderer = g_app_context->renderer.get();
    Skybox* skybox = g_skybox;
    
    // Show loading screen during initialization
    if (!initialization_complete) {
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        ImGui::SetNextWindowPos(ImVec2(WINDOW_WIDTH / 2 - 150, WINDOW_HEIGHT / 2 - 50));
        ImGui::SetNextWindowSize(ImVec2(300, 100));
        ImGui::Begin("Loading", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        ImGui::Text("Initializing Engine...");
        ImGui::ProgressBar(0.5f, ImVec2(-1, 0), "Loading Assets");
        ImGui::End();
        
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
        glFlush();
        glfwPollEvents();
        return;
    }
    
    updateFPS(window);
    
    if (!paused) {
        float yaw_rad = global_camera.yaw * M_PI / 180.0f;
        float sin_yaw = sinf(yaw_rad);
        float cos_yaw = cosf(yaw_rad);
        glm::vec3 cam_offset = glm::vec3(0.0f);
        float actual_cam_speed = tick_speed * cam_speed_multiplier;
        
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
            actual_cam_speed *= 2;
        }
        
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            cam_offset = cam_offset + glm::vec3(cos_yaw, 0, sin_yaw);
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            cam_offset = cam_offset + glm::vec3(-cos_yaw, 0, -sin_yaw);
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            cam_offset = cam_offset + glm::vec3(sin_yaw, 0, -cos_yaw);
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            cam_offset = cam_offset + glm::vec3(-sin_yaw, 0, cos_yaw);
        }

        if (glm::length(cam_offset) > 0.0f) {
            cam_offset = glm::normalize(cam_offset);
        }

        global_camera_vel.x += cam_offset.x * actual_cam_speed;
        global_camera_vel.z += cam_offset.z * actual_cam_speed;
        
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
            global_camera_vel.y += actual_cam_speed;
        }
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
            global_camera_vel.y -= actual_cam_speed;
        }

        global_camera_vel *= friction;
        global_camera.position = global_camera.position + global_camera_vel;
        
        view = camera_get_view_matrix(&global_camera);
        projection = camera_get_projection(&global_camera);
        
        // ============================================================================
        // UPDATE ENTITIES
        // ============================================================================

        entity_manager.updateEntity("cube", VEC3_NO_CHANGE, glm::vec3(update_count * 0.1f, update_count * 0.1f, update_count * 0.1f), VEC3_NO_CHANGE);
        entity_manager.updateEntity("sphere", glm::vec3(NAN, 2.5f + sinf(update_count * 0.01f), NAN), glm::vec3(update_count, 0, 0), VEC3_NO_CHANGE);
        entity_manager.updateEntity("statue", VEC3_NO_CHANGE, glm::vec3(NAN, update_count, NAN), VEC3_NO_CHANGE);
        entity_manager.updateEntity("instructions", glm::vec3(NAN, 2.0f + 0.05f * sinf(update_count * 0.05f), NAN), VEC3_NO_CHANGE, VEC3_NO_CHANGE);

        update_count += tick_speed;
    }
    
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // GPU timing queries - only on native platforms
    #ifndef __EMSCRIPTEN__
        glBeginQuery(GL_TIME_ELAPSED, shadowQueries[queryIndex]);
    #endif
    
    int shadowLightIndex = -1;
    for (size_t i = 0; i < lights.size(); i++) {
        renderer->renderShadowPass(entity_manager, lights[i]);
        shadowLightIndex = static_cast<int>(i);
        break;
    }
    
    #ifndef __EMSCRIPTEN__
        glEndQuery(GL_TIME_ELAPSED);
        glBeginQuery(GL_TIME_ELAPSED, skyboxQueries[queryIndex]);
    #endif
    
    skybox->render(&global_camera);
    
    #ifndef __EMSCRIPTEN__
        glEndQuery(GL_TIME_ELAPSED);
        glBeginQuery(GL_TIME_ELAPSED, mainQueries[queryIndex]);
    #endif
    
    renderer->setGlobalUniforms(global_camera);

    for (size_t i = 0; i < entity_manager.size(); i++) {
        Entity* entity = entity_manager.getEntityAt(i);
        if (entity && entity->active) {
            bool is_light = false;
            for (const auto& light : lights) {
                if (entity->name == light.entity_name) {
                    is_light = true;
                    for (const auto& meshPtr : entity->meshes) {
                        Mesh* mesh = meshPtr.get();
                        if (mesh && mesh->isValid()) {
                            renderer->drawUnlitMesh(entity, mesh, light.color, light.intensity);
                        }
                    }
                    break;
                }
            }
            
            if (!is_light) {
                renderer->drawEntity(entity, shadowLightIndex);
            }
        }
    }
    
    #ifndef __EMSCRIPTEN__
        glEndQuery(GL_TIME_ELAPSED);
        queryIndex = 1 - queryIndex;
        prevIndex = 1 - queryIndex;
    #endif

    glfwPollEvents();

    // ImGui UI
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::Begin("Engine");
    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Triangles: %u", total_triangles);
    
    #ifndef __EMSCRIPTEN__
        ImGui::Text("GPU Frame Time:");
        ImGui::Text("Shadows: %.3f ms", shadowTime);
        ImGui::Text("Skybox: %.3f ms", skyboxTime);
        ImGui::Text("Main: %.3f ms", mainTime);
        ImGui::Text("Total GPU: %.3f ms", shadowTime + skyboxTime + mainTime);
    #else
        ImGui::Text("(GPU timing disabled on Web)");
    #endif
    
    #ifndef __EMSCRIPTEN__
        if (ImGui::Button("V-Sync ON")) glfwSwapInterval(1);
        if (ImGui::Button("V-Sync OFF")) glfwSwapInterval(0);
    #endif
    ImGui::End();

    if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_NORMAL && !ImGui::GetIO().WantCaptureMouse) {
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            firstMouse = true;
            // Only set cursor mode, don't force disable immediately on web
            #ifdef __EMSCRIPTEN__
                // On web, this will trigger a pointer lock request which requires user gesture
                // The click itself provides the gesture
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            #else
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            #endif
            paused = false;
        }
    }
    
    #ifndef __EMSCRIPTEN__
        static bool fullscreen_toggle = false;
        if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS && glfwGetKey(window, GLFW_KEY_LEFT_ALT) && !fullscreen_toggle) {
            fullscreen_toggle = true;
            GLFWmonitor *monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode *vm = glfwGetVideoMode(monitor);
            if (!fullscreen) {
                WINDOW_WIDTH = vm->width;
                WINDOW_HEIGHT = vm->height;
                glfwSetWindowMonitor(window, monitor, 0, 0, vm->width, vm->height, vm->refreshRate);
                fullscreen = true;
            } else {
                WINDOW_WIDTH = 800;
                WINDOW_HEIGHT = 600;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                glfwSetWindowMonitor(window, nullptr, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
                fullscreen = false;
                int w, h;
                glfwGetWindowSize(window, &w, &h);
                GLFWmonitor *m = glfwGetPrimaryMonitor();
                int mx, my, mw, mh;
                glfwGetMonitorWorkarea(m, &mx, &my, &mw, &mh);
                glfwSetWindowPos(window, mx + (mw - w) / 2, my + (mh - h) / 2);
            }
        }
        if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_RELEASE && glfwGetKey(window, GLFW_KEY_LEFT_ALT)) fullscreen_toggle = false;
    #endif

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
    glFlush();
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================

int main() {
    
    printf("Starting OpenGL 3D Engine...\n");
    
    // Initialize GLFW
    if (!glfwInit()) {
        printf("Failed to initialize GLFW\n");
        return -1;
    }
    
    // Version number
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    
    // Remove resizability
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    
    // Anti-aliasing
    glfwWindowHint(GLFW_SAMPLES, 4);
    
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "OpenGL 3D Engine by @mu-gua-here", NULL, NULL);
    if (!window) {
        printf("Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    
    // Turn V-Sync off (not supported in Emscripten before main loop)
    #ifndef __EMSCRIPTEN__
        glfwSwapInterval(0);
    #endif
    
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        printf("Failed to initialize GLAD\n");
        return -1;
    }
    
    printf("OpenGL Version: %s\n", glGetString(GL_VERSION));
    
    #ifndef __EMSCRIPTEN__
        // Set up ImGui
        IMGUI_CHECKVERSION();
        std::remove("imgui.ini"); // Reset GUI menu state
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");
    #else
        // ImGui setup for Emscripten
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 300 es");
    #endif
    
    // Set callbacks
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    
    // Performance settings - GPU timing queries not supported on WebGL
    #ifndef __EMSCRIPTEN__
        glGenQueries(2, shadowQueries);
        glGenQueries(2, mainQueries);
        glGenQueries(2, skyboxQueries);
    #endif

    // Find executable path
    std::filesystem::path executable_path = getExecutablePath();

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
    global_camera = create_camera(static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT));
    camera_update_vectors(&global_camera);
    
    // Z buffer
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // OpenGL pixel functions
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // On web, multisampling cannot be toggled on or off at runtime
    #ifndef __EMSCRIPTEN__
        glEnable(GL_MULTISAMPLE);
    #endif
    
    // ============================================================================
    // LOAD ASSETS
    // ============================================================================
    
    // LOAD SKYBOXES //
    
    // Initialise skybox
    skyboxID = 0;
    #ifdef __EMSCRIPTEN__
        g_skybox = new Skybox();
    #else
        static Skybox skybox;
        g_skybox = &skybox;
    #endif
    g_skybox->initShader();
    
    // Cloud skybox
    std::string cloud_skybox_paths[6] = {
        buildAssetPath("res/skyboxes/Cloud_skybox/cloud_skybox_right.png"),   // GL_TEXTURE_CUBE_MAP_POSITIVE_X
        buildAssetPath("res/skyboxes/Cloud_skybox/cloud_skybox_left.png"),    // GL_TEXTURE_CUBE_MAP_NEGATIVE_X
        buildAssetPath("res/skyboxes/Cloud_skybox/cloud_skybox_top.png"),     // GL_TEXTURE_CUBE_MAP_POSITIVE_Y
        buildAssetPath("res/skyboxes/Cloud_skybox/cloud_skybox_bottom.png"),  // GL_TEXTURE_CUBE_MAP_NEGATIVE_Y
        buildAssetPath("res/skyboxes/Cloud_skybox/cloud_skybox_front.png"),   // GL_TEXTURE_CUBE_MAP_POSITIVE_Z
        buildAssetPath("res/skyboxes/Cloud_skybox/cloud_skybox_back.png")     // GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
    };
    
    const char* cloud_skybox[6] = {
        cloud_skybox_paths[0].c_str(),
        cloud_skybox_paths[1].c_str(),
        cloud_skybox_paths[2].c_str(),
        cloud_skybox_paths[3].c_str(),
        cloud_skybox_paths[4].c_str(),
        cloud_skybox_paths[5].c_str()
    };
    
    g_skybox->bindSkybox(cloud_skybox);
    
    // LOAD OBJ MESHES //

    printf("Loading meshes...\n");
    
    auto level_mesh = loadMesh("level/level.obj");    
    auto tree_mesh = loadMesh("realistic_tree/tree.obj");    
    auto instructions_mesh = loadMesh("instructions_panel/quad.obj");    
    auto cube_mesh = loadMesh("cube/cube.obj");    
    auto sphere_mesh = loadMesh("sphere/sphere.obj");    
    auto cone_mesh = loadMesh("cone/cone.obj");
    auto statue_mesh = loadMesh("statue/statue_of_myself.obj");
    auto plastic_table = loadMesh("plastic_table/plastic_table.obj");    
    auto road_mesh = loadMesh("modular_road/modular_road_pack.obj");
    auto character_idle = loadMesh("characters3d.com - Idle.fbx");
    
    printf("Meshes finished loading!\n");
    
    // ============================================================================
    // CREATE SCENE OBJECTS
    // ============================================================================
    
    // CREATE LIGHT SOURCES //
    
    createDirLight("sun", glm::vec3(1, -1, -1), glm::vec3(1, 1, 1), 10);
    createPointLight("streetlamp", glm::vec3(-7.05, 3.5, 0.05), glm::vec3(1, 1, 1), 250,
                    {}, glm::vec3(0.1, 0.1, 0.1), std::vector<int>{CULL_BACK});
    createSpotlight("torchlight", glm::vec3(0, 10, 0), glm::vec3(1, 1, 1), 50,
                    glm::vec3(0, -1, 0), 7.5f, 17.5f,
                    {}, glm::vec3(0.1, 0.1, 0.1), std::vector<int>{CULL_BACK});
    
    // Initialise shadow map
    initShadowMap();
    printf("Shadow map initialized successfully!\n");

    // CREATE ENTITIES //
    
    createEntity("level", std::move(level_mesh), glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), glm::vec3(10, 10, 10), std::vector<int> {CULL_NONE});
    createEntity("tree", std::move(tree_mesh), glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), std::vector<int>{CULL_BACK, CULL_NONE});
    createEntity("instructions", std::move(instructions_mesh), glm::vec3(0, 2, 4), glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), std::vector<int> {CULL_NONE});
    createEntity("cube", std::move(cube_mesh), glm::vec3(5, 3, 0), glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), std::vector<int> {CULL_BACK});
    createEntity("sphere", std::move(sphere_mesh), glm::vec3(0, 2, -5), glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), std::vector<int> {CULL_BACK});
    createEntity("cone", std::move(cone_mesh), glm::vec3(0, 10, 0), glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), std::vector<int>{CULL_BACK});
    createEntity("statue", std::move(statue_mesh), glm::vec3(-5, 1.9, -4), glm::vec3(0, 0, 0), glm::vec3(0.1, 0.1, 0.1), std::vector<int>{CULL_BACK});
    createEntity("plastic_table", std::move(plastic_table), glm::vec3(-5, 0, -4), glm::vec3(0, 0, 0), glm::vec3(0.5, 0.5, 0.5), std::vector<int>{CULL_BACK});
    createEntity("road", std::move(road_mesh), glm::vec3(-50, 0, 0), glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), std::vector<int>{CULL_BACK});
    // createEntity("character_idle", std::move(character_idle), glm::vec3(9, 0, 9), glm::vec3(0, 180, 0), glm::vec3(0.01, 0.01, 0.01), std::vector<int>{CULL_BACK});

    printf("Total triangles: %d\n", total_triangles);
    printf("Active entities: %zu\n", entity_manager.size());
    
    // Mark initialization as complete
    initialization_complete = true;
    printf("Initialization complete! Engine ready.\n");
    
    // ============================================================================
    // SETUP MAIN LOOP (both native and Emscripten)
    // ============================================================================
    
    // Create app context (used by both native and web)
    #ifdef __EMSCRIPTEN__
        g_app_context = new AppContext();
        g_app_context->window = window;
        g_app_context->renderer = std::move(renderer);
    #else
        AppContext app_ctx;
        app_ctx.window = window;
        app_ctx.renderer = std::move(renderer);
        g_app_context = &app_ctx;
    #endif
    
    #ifdef __EMSCRIPTEN__
        // Set Emscripten main loop but let browser handle the frame rate
        emscripten_set_main_loop(emscripten_main_loop_callback, 0, 1);
    #else
    // Native platform - traditional while loop
    while (!glfwWindowShouldClose(window)) {
        emscripten_main_loop_callback();
    }
    #endif
    
    // ============================================================================
    // CLEANUP
    // ============================================================================
    
    #ifndef __EMSCRIPTEN__
    printf("Cleaning up...\n");
    skybox.cleanup();
    
    if (default_texture_id != 0) {
        glDeleteTextures(1, &default_texture_id);
        default_texture_id = 0;
    }

    // Cleanup GPU timers
    glDeleteQueries(2, shadowQueries);
    glDeleteQueries(2, mainQueries);
    glDeleteQueries(2, skyboxQueries);
    
    cleanupShadowMap();
    
    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(window);
    glfwTerminate();
    #endif
    
    return 0;
}

// ============================================================================
// INPUT CALLBACKS
// ============================================================================

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    static double lastX = 400.0;
    static double lastY = 300.0;
    
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

    xoffset *= mouse_sensitivity;
    yoffset *= mouse_sensitivity;
    
    global_camera.yaw += xoffset;
    global_camera.pitch += yoffset;

    if (global_camera.pitch > 89.0f) global_camera.pitch = 89.0f;
    if (global_camera.pitch < -89.0f) global_camera.pitch = -89.0f;
    
    camera_update_vectors(&global_camera);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    WINDOW_WIDTH = width;
    WINDOW_HEIGHT = height;
    (void)window;
    glViewport(0, 0, width, height);
    if (height > 0) {
        global_camera.aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
    }
}
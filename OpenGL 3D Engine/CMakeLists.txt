cmake_minimum_required(VERSION 3.16)
project(OpenGL_3D_Engine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# macOS
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0")
    add_compile_options(-Wno-deprecated-declarations)
    add_definitions(-DGL_SILENCE_DEPRECATION)
endif()

# Compiler
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
add_definitions(-DGLFW_INCLUDE_NONE)

# Include dirs
include_directories(
    External/glad/include
    External/glfw/include
    External/glm
    External/stb
    imgui # imgui headers are here
)

# Executable
add_executable(${PROJECT_NAME}
    main.cpp
    External/glad/src/glad.c

    # ImGui core
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp

    # ImGui backends
    imgui/imgui_impl_glfw.cpp
    imgui/imgui_impl_opengl3.cpp
)

# Link
target_link_libraries(${PROJECT_NAME}
    "${CMAKE_SOURCE_DIR}/External/glfw/libglfw3.a"
    "-framework Cocoa"
    "-framework IOKit"
    "-framework CoreVideo"
    "-framework Metal"
    "-framework CoreFoundation"
    "-framework CoreGraphics"
)

# IDE helpers
if(CMAKE_GENERATOR STREQUAL "Xcode")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        XCODE_GENERATE_SCHEME TRUE
        XCODE_SCHEME_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
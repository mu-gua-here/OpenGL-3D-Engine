cmake_minimum_required(VERSION 3.16)

# Set the project name
project(OpenGL_3D_Engine)

# Set the C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set build type to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# macOS specific settings
if(APPLE)
    # Fix the macOS version warning
    set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0")
    
    # Suppress deprecation warnings
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
    
    # Add OpenGL silence macro
    add_definitions(-DGL_SILENCE_DEPRECATION)
    
    message(STATUS "macOS build configuration applied")
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Tell GLFW not to include gl.h
add_definitions(-DGLFW_INCLUDE_NONE)

# Include directories
include_directories(
    External/glad/include
    External/glfw/include
    External/glm
    External/stb
)

# Add the executable
add_executable(${PROJECT_NAME}
    main.cpp
    External/glad/src/glad.c
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    "${CMAKE_SOURCE_DIR}/External/glfw/libglfw3.a"
    "-framework Cocoa"
    "-framework IOKit"
    "-framework CoreVideo"
    "-framework Metal"
    "-framework CoreFoundation"
    "-framework CoreGraphics"
)

# BETTER ASSET HANDLING - Copy to single location
set(ASSETS_OUTPUT_DIR "${CMAKE_BINARY_DIR}")

# Copy assets to build directory
file(COPY "${CMAKE_SOURCE_DIR}/OBJ_models/" 
     DESTINATION "${ASSETS_OUTPUT_DIR}/OBJ_models/")
file(COPY "${CMAKE_SOURCE_DIR}/Skyboxes/" 
     DESTINATION "${ASSETS_OUTPUT_DIR}/Skyboxes/")

# For IDE generators (Xcode, Visual Studio), set working directory
if(CMAKE_GENERATOR STREQUAL "Xcode")
    # Tell Xcode to run from the build directory
    set_target_properties(${PROJECT_NAME} PROPERTIES
        XCODE_GENERATE_SCHEME TRUE
        XCODE_SCHEME_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
        
    message(STATUS "Xcode: Set working directory to ${CMAKE_BINARY_DIR}")
endif()

if(CMAKE_GENERATOR MATCHES "Visual Studio")
    # For Visual Studio, set the debugging working directory
    set_target_properties(${PROJECT_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
        
    message(STATUS "Visual Studio: Set working directory to ${CMAKE_BINARY_DIR}")
endif()

# Print debug information
message(STATUS "=== BUILD CONFIGURATION ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Generator: ${CMAKE_GENERATOR}")
message(STATUS "Source dir: ${CMAKE_SOURCE_DIR}")
message(STATUS "Binary dir: ${CMAKE_BINARY_DIR}")
message(STATUS "Assets will be copied to: ${ASSETS_OUTPUT_DIR}")

# Verify source assets exist
if(EXISTS "${CMAKE_SOURCE_DIR}/OBJ_models/")
    message(STATUS "✓ Found OBJ_models directory")
else()
    message(WARNING "✗ OBJ_models directory not found at source!")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/Skyboxes/")
    message(STATUS "✓ Found Skyboxes directory") 
else()
    message(WARNING "✗ Skyboxes directory not found at source!")
endif()

# Optional: Create a custom target to copy assets during build
add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/OBJ_models"
        "${ASSETS_OUTPUT_DIR}/OBJ_models"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Skyboxes"
        "${ASSETS_OUTPUT_DIR}/Skyboxes"
    COMMENT "Copying assets to build directory"
)

# Make sure assets are copied before building the executable
add_dependencies(${PROJECT_NAME} copy_assets)

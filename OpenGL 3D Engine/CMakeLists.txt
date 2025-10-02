cmake_minimum_required(VERSION 3.16)

# Set the project name
project(OpenGL_3D_Engine)

# Set the C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
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

# Verify source assets exist
if(EXISTS "${CMAKE_SOURCE_DIR}/OBJ_Models/")
    message(STATUS "Found OBJ_Models directory")
else()
    message(WARNING "OBJ_Models directory not found at source")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/Skyboxes/")
    message(STATUS "Found Skyboxes directory")
else()
    message(WARNING "Skyboxes directory not found at source")
endif()

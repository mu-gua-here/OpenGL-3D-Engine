cmake_minimum_required(VERSION 3.16)
project(OpenGL-3D-Engine VERSION 0.0.0)

# Basic settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform-specific settings
if(APPLE)
    add_compile_options(-Wno-deprecated-declarations)
    add_definitions(-DGL_SILENCE_DEPRECATION)
endif()

# Compiler flags
if(MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /DEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
    add_definitions(-D_USE_MATH_DEFINES)
    add_compile_options(/wd4244)
else()
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
endif()

# Output directories (simplified)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Ensure multi-config support
if(NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
endif()

# Set output directories for multi-config
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/bin/${CONFIG})
endforeach()

# Executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    external/glad/src/glad.c
    external/imgui/imgui.cpp
    external/imgui/imgui_demo.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/imgui_impl_glfw.cpp
    external/imgui/imgui_impl_opengl3.cpp
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    external/glad/include
    external/glfw/include
    external/glm
    external/stb
    external/imgui
    external/assimp/include
)

# Link libraries
if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "${CMAKE_SOURCE_DIR}/external/glfw/libglfw3.a"
        "${CMAKE_SOURCE_DIR}/external/assimp/libassimp.a"
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
        "-lz"
    )
elseif(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "${CMAKE_SOURCE_DIR}/external/glfw/glfw3.lib"
        "${CMAKE_SOURCE_DIR}/external/assimp/assimp.lib"
        opengl32
    )
else() # Linux
    find_package(OpenGL REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        OpenGL::GL
        "${CMAKE_SOURCE_DIR}/external/glfw/libglfw3.a"
        "${CMAKE_SOURCE_DIR}/external/assimp/libassimp.a"
        dl
        pthread
    )
endif()

# Ensure target properties are set for multi-config
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Installation rules
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ".")
install(DIRECTORY 
    "${CMAKE_SOURCE_DIR}/scene_models"
    "${CMAKE_SOURCE_DIR}/skyboxes"
    DESTINATION "."
)
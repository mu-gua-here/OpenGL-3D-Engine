cmake_minimum_required(VERSION 3.16)
project(OpenGL-3D-Engine VERSION 0.0.0)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release/lib")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)

# macOS
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0")
    add_compile_options(-Wno-deprecated-declarations)
    add_definitions(-DGL_SILENCE_DEPRECATION)
endif()

# Compiler
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")

# Use -O3 for GNU/Clang, but use /O2 for MSVC (Windows)
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
endif()

if(MSVC)
    add_definitions(-D_USE_MATH_DEFINES)
    add_compile_options(/wd4244) # Suppress potential truncation warnings
endif()

# Executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    external/glad/src/glad.c

    # ImGui core
    external/imgui/imgui.cpp
    external/imgui/imgui_demo.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp

    # ImGui backends
    external/imgui/imgui_impl_glfw.cpp
    external/imgui/imgui_impl_opengl3.cpp
)

target_compile_definitions(${PROJECT_NAME} PRIVATE GLFW_INCLUDE_NONE)

# Include dirs
target_include_directories(${PROJECT_NAME} PRIVATE
    external/glad/include
    external/glfw/include
    external/glm
    external/stb
    external/imgui
    external/assimp/include
)

if(NOT APPLE)
    find_package(OpenGL REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL)
endif()

# Replace the existing target_link_libraries with:
if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "${CMAKE_SOURCE_DIR}/external/glfw/libglfw3.a"
        "${CMAKE_SOURCE_DIR}/external/assimp/libassimp.a"
        "-lz"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework Metal"
        "-framework CoreFoundation"
        "-framework CoreGraphics"
        "-framework OpenGL"
    )
endif()

# IDE helpers
if(CMAKE_GENERATOR STREQUAL "Xcode")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        XCODE_GENERATE_SCHEME TRUE
        XCODE_SCHEME_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets" DESTINATION ".")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/shaders" DESTINATION ".")
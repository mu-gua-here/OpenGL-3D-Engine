cmake_minimum_required(VERSION 3.16)
project(OpenGL-3D-Engine VERSION 0.0.0 LANGUAGES C CXX)

# Basic settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Platform-specific settings
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures for macOS")
    add_compile_options(-Wno-deprecated-declarations)
    add_definitions(-DGL_SILENCE_DEPRECATION)
endif()

# Compiler flags
if(MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /DEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
    add_definitions(-D_USE_MATH_DEFINES -DNOMINMAX)
    add_compile_options(/wd4244 /wd4005 /W3)
else()
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Multi-config support
if(NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
endif()

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

# macOS-specific Info.plist
if(APPLE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist")
    target_link_options(${PROJECT_NAME} PRIVATE
        -Wl,-sectcreate,__TEXT,__info_plist,${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
    )
endif()

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    external/glad/include
    external/glm
    external/stb
    external/imgui
)

# Library detection and linking
if(WIN32)
    # Windows - prefer vcpkg if available
    find_package(glfw3 CONFIG QUIET)
    find_package(assimp CONFIG QUIET)
    
    if(glfw3_FOUND AND assimp_FOUND)
        message(STATUS "Using vcpkg/system libraries for Windows")
        target_link_libraries(${PROJECT_NAME} PRIVATE
            glfw
            assimp::assimp
            opengl32
        )
    else()
        # Fallback to local precompiled (development only)
        message(STATUS "Using local precompiled libraries for Windows")
        target_include_directories(${PROJECT_NAME} PRIVATE
            external/glfw/include
            external/assimp/include
        )
        target_link_libraries(${PROJECT_NAME} PRIVATE
            "${CMAKE_SOURCE_DIR}/external/glfw/glfw3.lib"
            "${CMAKE_SOURCE_DIR}/external/assimp/assimp.lib"
            opengl32
        )
    endif()
    
elseif(APPLE AND GLFW_LIBRARY AND ASSIMP_LIBRARY)
    # macOS with Homebrew (CI build)
    message(STATUS "Using Homebrew libraries for macOS CI")
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${GLFW_INCLUDE_DIR}
        ${ASSIMP_INCLUDE_DIR}
    )
    
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${GLFW_LIBRARY}
        ${ASSIMP_LIBRARY}
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
    
elseif(UNIX AND NOT APPLE)
    # Linux CI Build - Use explicit paths passed via the YAML script's -D flags
    find_package(OpenGL REQUIRED)

    message(STATUS "Manually linking system libraries for Linux via YAML passed variables")
    
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${GLFW_INCLUDE_DIR}
        ${ASSIMP_INCLUDE_DIR}
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE
        OpenGL::GL
        glfw
        assimp
        ${CMAKE_DL_LIBS}
        pthread
    )
    
else()
    # Fallback to local precompiled libraries (development)
    message(STATUS "Using local precompiled libraries")
    target_include_directories(${PROJECT_NAME} PRIVATE
        external/glfw/include
        external/assimp/include
    )
    
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
    else()
        find_package(OpenGL REQUIRED)
        target_link_libraries(${PROJECT_NAME} PRIVATE
            OpenGL::GL
            "${CMAKE_SOURCE_DIR}/external/glfw/libglfw3.a"
            "${CMAKE_SOURCE_DIR}/external/assimp/libassimp.a"
            ${CMAKE_DL_LIBS}
            pthread
        )
    endif()
endif()

# Target properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Installation rules
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION .
)

install(DIRECTORY 
    "${CMAKE_SOURCE_DIR}/scene_models"
    "${CMAKE_SOURCE_DIR}/skyboxes"
    DESTINATION "."
)

# Print configuration summary
message(STATUS "=== Build Configuration ===")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
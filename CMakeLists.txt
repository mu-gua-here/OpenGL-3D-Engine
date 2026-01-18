cmake_minimum_required(VERSION 3.16)

# EMSCRIPTEN DETECTION - MUST BE BEFORE project()

if(DEFINED ENV{EMSCRIPTEN})
    set(EMSCRIPTEN TRUE)
    message(STATUS "Detected Emscripten build environment")
    # Clear macOS-specific settings
    set(CMAKE_OSX_ARCHITECTURES "" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "" CACHE STRING "" FORCE)
    set(CMAKE_OSX_SYSROOT "" CACHE STRING "" FORCE)
endif()

project(OpenGL-3D-Engine LANGUAGES C CXX)

# Basic settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Platform-specific settings (only for native builds)
if(APPLE AND NOT EMSCRIPTEN)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures for macOS")
    add_compile_options(-Wno-deprecated-declarations)
    add_compile_options(-Wno-deprecated-non-prototype)
    add_definitions(-DGL_SILENCE_DEPRECATION)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-deprecated-non-prototype")
endif()

# Compiler flags
if(MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /DEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Oi /Ot /GL /DNDEBUG")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
    add_definitions(-D_USE_MATH_DEFINES -DNOMINMAX)
    add_compile_options(/wd4244 /wd4005 /W3)
elseif(NOT EMSCRIPTEN)
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -flto -ffast-math")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-flto")
    
    # Add these macOS-specific optimizations
    if(APPLE)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mtune=native")
    endif()
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# ============================================================================
# EXECUTABLE
# ============================================================================

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/shader_loading.cpp
    src/material.cpp
    src/filesystem.cpp
    src/skybox.cpp
    src/camera.cpp
    src/texture_loader.cpp
    src/mesh_loader.cpp
    src/light.cpp
    src/entity_manager.cpp
    src/renderer.cpp
    src/shadowmap.cpp
    lib/glad/src/glad.c
    lib/imgui/imgui.cpp
    lib/imgui/imgui_demo.cpp
    lib/imgui/imgui_draw.cpp
    lib/imgui/imgui_tables.cpp
    lib/imgui/imgui_widgets.cpp
    lib/imgui/imgui_impl_glfw.cpp
    lib/imgui/imgui_impl_opengl3.cpp
)

# ============================================================================
# EMSCRIPTEN CONFIGURATION (after add_executable)
# ============================================================================

if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    
    # Build link flags
    set(EMSCRIPTEN_LINK_FLAGS "")
    list(APPEND EMSCRIPTEN_LINK_FLAGS "--preload-file \"${CMAKE_SOURCE_DIR}/res@/res\"")
    list(APPEND EMSCRIPTEN_LINK_FLAGS "-sALLOW_MEMORY_GROWTH=1")
    list(APPEND EMSCRIPTEN_LINK_FLAGS "-sMAX_WEBGL_VERSION=2")
    list(APPEND EMSCRIPTEN_LINK_FLAGS "-sUSE_GLFW=3")
    list(APPEND EMSCRIPTEN_LINK_FLAGS "-sUSE_WEBGL2=1")
    list(APPEND EMSCRIPTEN_LINK_FLAGS "-sWASM=1")
    list(APPEND EMSCRIPTEN_LINK_FLAGS "-sFULL_ES3=1")

    # Debugging flags
    list(APPEND EMSCRIPTEN_LINK_FLAGS "-sASSERTIONS=2")  # Enable assertions
    list(APPEND EMSCRIPTEN_LINK_FLAGS "-sSAFE_HEAP=1")   # Check for bad memory access
    list(APPEND EMSCRIPTEN_LINK_FLAGS "-sSTACK_OVERFLOW_CHECK=2")  # Detect stack overflow
    list(APPEND EMSCRIPTEN_LINK_FLAGS "-sNO_DISABLE_EXCEPTION_CATCHING")
    
    string(REPLACE ";" " " EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS}")
    
    set_target_properties(${PROJECT_NAME} PROPERTIES
        LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS}"
    )
    
    # Build Assimp from source for Emscripten
    include(FetchContent)
    
    message(STATUS "Fetching Assimp for Emscripten...")
    
    # Disable strict warnings for Assimp
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=nontrivial-memcall")
    
    # Set variables before FetchContent_Declare!
    set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
    set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)
    set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    
    FetchContent_Declare(
        assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG v5.4.3
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(assimp)
    
    target_link_libraries(${PROJECT_NAME} PRIVATE assimp)
    
    message(STATUS "✓ Emscripten build configured with Assimp from source")
endif()

# macOS-specific Info.plist
if(APPLE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist")
    target_link_options(${PROJECT_NAME} PRIVATE
        -Wl,-sectcreate,__TEXT,__info_plist,${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
    )
endif()

# Include directories
target_include_directories(
    ${PROJECT_NAME} PRIVATE 
    include/
    lib/glad/include
    lib/glm
    lib/stb
    lib/imgui
)

# ============================================================================
# LIBRARY DETECTION AND LINKING (NATIVE ONLY)
# ============================================================================

if(NOT EMSCRIPTEN)
    message(STATUS "Configuring for native platform")
    
    set(USING_SYSTEM_LIBS FALSE)
    
    # Try to find system libraries
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(ASSIMP QUIET assimp)
        pkg_check_modules(GLFW QUIET glfw3)
    endif()
    
    # Also try CMake's find_package (for vcpkg/Homebrew)
    if(NOT ASSIMP_FOUND)
        find_package(assimp QUIET)
        if(assimp_FOUND)
            set(ASSIMP_FOUND TRUE)
        endif()
    endif()
    
    if(NOT GLFW_FOUND)
        find_package(glfw3 QUIET)
        if(glfw3_FOUND)
            set(GLFW_FOUND TRUE)
        endif()
    endif()
    
    # Check if we have both system libraries
    if(ASSIMP_FOUND AND GLFW_FOUND)
        set(USING_SYSTEM_LIBS TRUE)
        message(STATUS "✓ Using system libraries (Homebrew/vcpkg)")
    else()
        message(STATUS "✗ System libraries not found, will fetch from source")
    endif()
    
    # Now link or fetch based on what we found
    if(USING_SYSTEM_LIBS)
        # Use system libraries
        if(TARGET assimp::assimp)
            target_link_libraries(${PROJECT_NAME} PRIVATE assimp::assimp)
        elseif(ASSIMP_LIBRARIES)
            target_include_directories(${PROJECT_NAME} PRIVATE ${ASSIMP_INCLUDE_DIRS})
            target_link_libraries(${PROJECT_NAME} PRIVATE ${ASSIMP_LIBRARIES})
        endif()
        
        if(TARGET glfw)
            target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
        elseif(GLFW_LIBRARIES)
            target_include_directories(${PROJECT_NAME} PRIVATE ${GLFW_INCLUDE_DIRS})
            target_link_libraries(${PROJECT_NAME} PRIVATE ${GLFW_LIBRARIES})
        endif()
        
    else()
        # Fetch from source
        include(FetchContent)
        
        message(STATUS "Fetching GLFW...")
        FetchContent_Declare(
            glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG 3.3.8
            GIT_SHALLOW TRUE
        )
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(glfw)
        
        message(STATUS "Fetching Assimp...")
        FetchContent_Declare(
            assimp
            GIT_REPOSITORY https://github.com/assimp/assimp.git
            GIT_TAG v5.4.3
            GIT_SHALLOW TRUE
        )
        set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
        set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
        set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(assimp)
        
        target_link_libraries(${PROJECT_NAME} PRIVATE
            glfw
            assimp
        )
    endif()
    
    # Platform-specific system libraries
    if(APPLE)
        target_link_libraries(${PROJECT_NAME} PRIVATE
            "-framework OpenGL"
            "-framework Cocoa"
            "-framework IOKit"
            "-framework CoreVideo"
        )
    elseif(WIN32)
        target_link_libraries(${PROJECT_NAME} PRIVATE opengl32)
    else()
        find_package(OpenGL REQUIRED)
        target_link_libraries(${PROJECT_NAME} PRIVATE 
            OpenGL::GL
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
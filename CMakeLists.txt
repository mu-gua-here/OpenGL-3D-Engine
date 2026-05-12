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

project(OpenGL3DEngine LANGUAGES C CXX)

# Clean name for target and output
set(TARGET_NAME OpenGL-3D-Engine)

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
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# ============================================================================
# EXECUTABLE
# ============================================================================

add_executable(${TARGET_NAME}
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
    src/bloom.cpp
    src/physics.cpp
    src/object_picking.cpp
    lib/glad/src/glad.c
    lib/imgui/imgui.cpp
    lib/imgui/imgui_demo.cpp
    lib/imgui/imgui_draw.cpp
    lib/imgui/imgui_tables.cpp
    lib/imgui/imgui_widgets.cpp
    lib/imgui/imgui_impl_glfw.cpp
    lib/imgui/imgui_impl_opengl3.cpp
)

if(NOT EMSCRIPTEN)
    # Native builds: use GLAD as the loader for the ImGui OpenGL3 backend
    target_compile_definitions(${TARGET_NAME} PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)
else()
    # Web builds: use the GLES path (GLAD isn’t used with Emscripten)
    target_compile_definitions(${TARGET_NAME} PRIVATE IMGUI_IMPL_OPENGL_ES3)
endif()

# ============================================================================
# EMSCRIPTEN CONFIGURATION
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

    set(_ems_flags "")
    foreach(dir res level realistic_tree instructions_panel cube sphere cone statue plastic_table)
        list(APPEND _ems_flags "--preload-file \"${CMAKE_SOURCE_DIR}/${dir}@/${dir}\"")
    endforeach()
    string(REPLACE ";" " " _ems_flags "${_ems_flags}")
    set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS "${_ems_flags} -sUSE_GLFW=3 -sFULL_ES3=1 -sALLOW_MEMORY_GROWTH=1 -sWASM=1 -sASSERTIONS=2 -sSAFE_HEAP=1 -sSTACK_OVERFLOW_CHECK=2 -sNO_DISABLE_EXCEPTION_CATCHING")
    
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
    
    target_link_libraries(${TARGET_NAME} PRIVATE assimp)
    
    message(STATUS "✓ Emscripten build configured with Assimp from source")
endif()

# macOS-specific Info.plist
if(APPLE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist")
    target_link_options(${TARGET_NAME} PRIVATE
        -Wl,-sectcreate,__TEXT,__info_plist,${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
    )
endif()

# Include directories
target_include_directories(
    ${TARGET_NAME} PRIVATE 
    include/
    lib/glad/include
    lib/glm
    lib/stb
    lib/imgui
)

# Collect header files so IDEs (Xcode/VS) show them in the project
file(GLOB_RECURSE PROJECT_HEADERS
    "${CMAKE_SOURCE_DIR}/include/*.h"
    "${CMAKE_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_SOURCE_DIR}/lib/**/*.h"
    "${CMAKE_SOURCE_DIR}/lib/**/*.hpp"
)
if(PROJECT_HEADERS)
    target_sources(${TARGET_NAME} PRIVATE ${PROJECT_HEADERS})
endif()

# Collect resources (shaders, models, skyboxes, res/) and preserve
# on-disk directory structure in the Xcode project and app bundle.
file(GLOB_RECURSE RESOURCE_FILES_REL RELATIVE ${CMAKE_SOURCE_DIR}
    "res/*"
    "shaders/*"
    "skyboxes/*"
    "scene_models/*"
)
set(RESOURCE_FULL_PATHS)
foreach(_rel IN LISTS RESOURCE_FILES_REL)
    list(APPEND RESOURCE_FULL_PATHS "${CMAKE_SOURCE_DIR}/${_rel}")
endforeach()

if(RESOURCE_FULL_PATHS)
    if(CMAKE_GENERATOR STREQUAL "Xcode")
        foreach(_rel IN LISTS RESOURCE_FILES_REL)
            get_filename_component(_dir "${_rel}" PATH)
            if(_dir STREQUAL "")
                set(_mac_loc "Resources")
            else()
                string(REPLACE "\\" "/" _dir_unix "${_dir}")
                set(_mac_loc "Resources/${_dir_unix}")
            endif()
            set(_full "${CMAKE_SOURCE_DIR}/${_rel}")
            set_source_files_properties(${_full} PROPERTIES MACOSX_PACKAGE_LOCATION "${_mac_loc}")
        endforeach()
    endif()

    # Only add resource files as sources for Xcode (for bundling) or Emscripten (for preloading)
    # For other generators like Ninja, these are runtime-loaded files, not sources
    if(CMAKE_GENERATOR STREQUAL "Xcode" OR EMSCRIPTEN)
        target_sources(${TARGET_NAME} PRIVATE ${RESOURCE_FULL_PATHS})
    endif()

    # Show them in Xcode/VS grouped by folder structure
    source_group(TREE ${CMAKE_SOURCE_DIR} PREFIX "Resources" FILES ${RESOURCE_FULL_PATHS})
endif()

# Organize files into logical groups for IDEs (Xcode/Visual Studio)
source_group(TREE ${CMAKE_SOURCE_DIR}/src PREFIX "Sources" FILES
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
    src/bloom.cpp
)
source_group(TREE ${CMAKE_SOURCE_DIR} PREFIX "Headers" FILES ${PROJECT_HEADERS})
source_group(TREE ${CMAKE_SOURCE_DIR}/lib PREFIX "ThirdParty" FILES
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
            target_link_libraries(${TARGET_NAME} PRIVATE assimp::assimp)
        elseif(ASSIMP_LIBRARIES)
            target_include_directories(${TARGET_NAME} PRIVATE ${ASSIMP_INCLUDE_DIRS})
            target_link_libraries(${TARGET_NAME} PRIVATE ${ASSIMP_LIBRARIES})
        endif()
        
        if(TARGET glfw)
            target_link_libraries(${TARGET_NAME} PRIVATE glfw)
        elseif(GLFW_LIBRARIES)
            target_include_directories(${TARGET_NAME} PRIVATE ${GLFW_INCLUDE_DIRS})
            target_link_libraries(${TARGET_NAME} PRIVATE ${GLFW_LIBRARIES})
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
        
        target_link_libraries(${TARGET_NAME} PRIVATE
            glfw
            assimp
        )
    endif()
    
    # Platform-specific system libraries
    if(APPLE)
        target_link_libraries(${TARGET_NAME} PRIVATE
            "-framework OpenGL"
            "-framework Cocoa"
            "-framework IOKit"
            "-framework CoreVideo"
        )
    elseif(WIN32)
        target_link_libraries(${TARGET_NAME} PRIVATE opengl32)
    else()
        find_package(OpenGL REQUIRED)
        target_link_libraries(${TARGET_NAME} PRIVATE 
            OpenGL::GL
            ${CMAKE_DL_LIBS}
            pthread
        )
    endif()
endif()

# Target properties
if(CMAKE_GENERATOR STREQUAL "Xcode")
    message(STATUS "Configuring for Xcode")
    # Xcode-specific properties (e.g., working directory for your scheme)
    set_target_properties(${TARGET_NAME} PROPERTIES
        XCODE_SCHEME_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
elseif(CMAKE_GENERATOR MATCHES "Ninja|Unix Makefiles")
    message(STATUS "Configuring for Ninja/Makefiles (likely VS Code)")
    set_target_properties(${TARGET_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        BUILD_RPATH "/opt/homebrew/lib;/usr/local/lib"
        INSTALL_RPATH "/opt/homebrew/lib;/usr/local/lib"
    )
endif()

# Installation rules
install(TARGETS ${TARGET_NAME}
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
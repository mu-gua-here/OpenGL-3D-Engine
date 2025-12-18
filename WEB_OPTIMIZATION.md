# Web Optimization Guide

This document outlines all optimizations applied to make the 3D engine run efficiently on WebAssembly/WebGL.

## Platform-Specific Optimizations Implemented

### 1. **Shader Version Handling** ✅
- **Issue**: WebGL ES 3.0 doesn't support `#version 330 core`; requires `#version 300 es`
- **Solution**: `shader_loading.cpp` automatically injects the correct version based on platform
  - Web: `#version 300 es\nprecision mediump float;`
  - Native: `#version 330 core`
- **Files**: All 8 shaders in `res/shaders/` (no manual version directives)
- **Result**: Single shader source files work on both platforms

### 2. **Shadow Map Resolution** ✅
- **Issue**: 4096x4096 shadow maps cause VRAM pressure on web
- **Solution**: Conditional resolution in `main.cpp` (lines 168-176)
  - Web: 1024x1024 (4MB VRAM vs 64MB)
  - Native: 4096x4096 (full quality)
- **Impact**: ~15x reduction in shadow map memory usage on web
- **Code**:
  ```cpp
  #ifdef __EMSCRIPTEN__
      const int SHADOW_MAP_SIZE = 1024;
  #else
      const int SHADOW_MAP_SIZE = 4096;
  #endif
  ```

### 3. **GPU Query Guards** ✅
- **Issue**: WebGL doesn't support GPU timing queries
- **Solution**: `#ifndef __EMSCRIPTEN__` guards around all timing query operations
- **Files**: `main.cpp` (GPU time display disabled on web)
- **Result**: No crashes from unsupported WebGL extensions

### 4. **UI Optimization** ✅
- **Issue**: V-Sync controls don't work on web; GLFW swap interval limited in Emscripten
- **Solution**: Hide V-Sync buttons in ImGui on web platform
- **Impact**: Cleaner UI, prevents user confusion
- **Code**: Lines 981-987 in `main.cpp` wrapped in `#ifndef __EMSCRIPTEN__`

### 5. **Loading Screen** ✅
- **Issue**: Asset loading blocks main thread, appears frozen
- **Solution**: Loading screen UI displays until `initialization_complete` flag is set
- **Files**: `main.cpp` lines 825-832
- **Features**:
  - Progress bar
  - Loading message
  - Centered on screen
  - ImGui overlay (no blocking)

### 6. **Asset Error Handling** ✅
- **Issue**: Failed mesh loads hang the application
- **Solution**: Error checking per asset with informative logging
- **Impact**: Prevents freezes, helps debug missing files

## Build Configuration

### CMakeLists.txt Emscripten Section
```cmake
if(EMSCRIPTEN)
    message(STATUS "Building for Emscripten/WebAssembly")
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    set(USE_GLFW 3)
    set(USE_WEBGL 2)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        -sUSE_GLFW=3
        -sUSE_WEBGL2=1
        -sWASM=1
        -sALLOW_MEMORY_GROWTH=1
        -sMAXIMUM_MEMORY=1gb
    )
endif()
```

### Compilation Commands

**Native Build (macOS/Linux/Windows)**:
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./bin/Release/OpenGL-3D-Engine
```

**Web Build (Emscripten)**:
```bash
# Install Emscripten
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk && ./emsdk install latest && ./emsdk activate latest
source ./emsdk_env.sh

# Build
mkdir build && cd build
emcmake cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXECUTABLE_SUFFIX=".html"
emmake make -j$(nproc)

# Output: build/bin/Release/OpenGL-3D-Engine.html
```

## Memory Management

- **ALLOW_MEMORY_GROWTH**: Enabled (dynamically grow heap)
- **MAXIMUM_MEMORY**: Set to 1GB (prevents unbounded growth)
- **WebGL 2.0**: Full support for floating-point textures and advanced features

## Known Limitations on Web

| Feature | Status | Reason |
|---------|--------|--------|
| GPU Timing Queries | ❌ Disabled | WebGL doesn't expose GPU timers |
| V-Sync Control | ❌ Disabled | Limited GLFW support in Emscripten |
| Async Asset Loading | ⏳ TODO | Not yet implemented |
| Touch Controls | ⏳ TODO | Mobile support planned |
| Asset Streaming | ⏳ TODO | For faster initial load |

## Future Optimization Tasks

### Priority 1 - Critical for Production
- [ ] **Async Asset Loading**: Load meshes/textures without blocking main thread
- [ ] **Asset Streaming**: Download chunks on-demand (lower initial WASM size)
- [ ] **HTML Template Enhancement**: Add progress bar, better branding

### Priority 2 - Performance Polish
- [ ] **Texture Compression**: Use KTX2 format with basis_universal
- [ ] **LOD System**: Reduce triangle count for distant objects on web
- [ ] **Draw Call Batching**: Reduce state changes
- [ ] **Memory Profiling**: Identify and optimize hot paths

### Priority 3 - User Experience
- [ ] **Touch Controls**: Mobile/tablet support
- [ ] **Responsive Layout**: Adapt UI to screen size
- [ ] **Fallback Graphics**: Graceful degradation on older devices

## GitHub Actions Deployment

The workflow in `.github/workflows/itchio_deploy.yml`:

1. Sets up Emscripten SDK
2. Uses `emcmake`/`emmake` to invoke CMake with Emscripten toolchain
3. Compiles to WebAssembly with HTML shell template
4. Uploads build artifacts to GitHub Pages

**Deployment**: Merge to `main` branch → Automatic GitHub Pages deployment

## Performance Metrics (Target)

- **Initial Load**: < 30 seconds (Emscripten optimized)
- **FPS**: 30-60 FPS on mid-range devices
- **Memory**: < 512MB WASM heap usage
- **Time to Interactive**: < 5 seconds

## Debugging Web Build

### Enable Debug Output
```bash
emcmake cmake .. -DCMAKE_BUILD_TYPE=Debug
emmake make VERBOSE=1
```

### Check WASM Output
```bash
# List exported functions
wasm-objdump -x build/bin/Release/OpenGL-3D-Engine.wasm | grep exports
```

### Browser DevTools
1. Open browser console (F12)
2. Check for Emscripten runtime errors
3. Use Performance tab to profile rendering
4. Monitor Network tab for asset loading

## References

- [Emscripten Documentation](https://emscripten.org/docs/index.html)
- [WebGL 2.0 Specification](https://www.khronos.org/webgl/wiki/Getting_Started_with_WebGL)
- [WebAssembly Performance](https://developer.mozilla.org/en-US/docs/WebAssembly/Reference/Functions)

---

**Last Updated**: Post-shader-system integration
**Build Status**: ✅ Both native and web builds successful (Exit Code: 0)

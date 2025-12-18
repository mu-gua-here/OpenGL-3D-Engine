# Emscripten/WebGL Migration Guide

## Changes Made

### 1. **CMakeLists.txt**
- Added Emscripten detection with `if(EMSCRIPTEN)`
- Set executable suffix to `.html` for Emscripten builds
- Added Emscripten-specific compiler flags:
  - `-sUSE_GLFW=3` - Use Emscripten's GLFW port
  - `-sUSE_WEBGL2=1` - Use WebGL 2.0 (ES 3.0 equivalent)
  - `-sWASM=1` - Enable WebAssembly
  - `-sALLOW_MEMORY_GROWTH=1` - Allow dynamic memory growth
  - `-sMAXIMUM_MEMORY=1gb` - Set max memory to 1GB

### 2. **main.cpp - Platform Headers**
- Added `#ifdef __EMSCRIPTEN__` block with:
  - `#include <emscripten.h>`
  - `#include <emscripten/html5.h>`

### 3. **main.cpp - App Context Structure**
Created a new `AppContext` struct to manage global state in Emscripten:
```cpp
struct AppContext {
    GLFWwindow* window = nullptr;
    std::unique_ptr<Renderer> renderer;
    Skybox skybox;
    bool should_close = false;
};
```

### 4. **main.cpp - Main Loop Refactoring**
**Critical Change**: Converted the traditional `while()` loop to Emscripten's event-based model:

- **Original**: `while (!glfwWindowShouldClose(window)) { ... }`
- **New**: `emscripten_set_main_loop(emscripten_main_loop_callback, 60, 1);`

The entire main loop logic is now in `emscripten_main_loop_callback()`, which is called ~60 times per second by the browser.

### 5. **main.cpp - Platform-Specific Code**
Added conditional compilation guards:
- Fullscreen toggling **disabled** on Emscripten (browser controls fullscreen)
- All window management features still work on native platforms

## What Still Needs to Be Done

### 1. **Shader Compatibility**
WebGL 2.0 (ES 3.0) has some differences from OpenGL 3.3:
- May need to adjust shader syntax (precision qualifiers, etc.)
- Check your `pbr.fs`, `pbr.vs`, `shadow.fs`, `skybox.fs` for incompatibilities

### 2. **Asset Loading Paths**
When building with Emscripten using `--preload-file`, assets are available at:
- `/res/shaders/...`
- `/res/skyboxes/...`
- `/res/scene_models/...`

Your current `buildAssetPath()` and `loadShaderFile()` functions need testing to ensure they work with Emscripten's virtual filesystem.

### 3. **Performance Considerations**
- GPU queries for timing may not work the same in WebGL
- Consider disabling or modifying the GPU timing display
- WebGL 2.0 doesn't support all OpenGL features:
  - `glBeginQuery()` / `glEndQuery()` - May not work
  - Some texture formats may differ
  - Shadow map resolution might need reduction

### 4. **YAML Workflow Update**
Your GitHub Actions workflow needs adjustment:
```yaml
- name: Build WebAssembly
  run: |
    mkdir -p build
    cd build
    emcmake cmake .. \
      -DCMAKE_TOOLCHAIN_FILE=$EMSDK/cmake/Modules/Platform/Emscripten.cmake \
      -DCMAKE_BUILD_TYPE=Release
    emmake make -j4
```

### 5. **HTML Template**
Create/update your `shell_minimal.html` with:
- Canvas element for rendering
- Loading screen UI
- Error handling for WASM loading failures
- Mobile-friendly input handling (touch controls for camera)

## Building for Emscripten

```bash
# Download and setup Emscripten SDK
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh

# Build your project
cd /path/to/OpenGL-3D-Engine
mkdir -p build_web
cd build_web

# Use Emscripten CMake toolchain
emcmake cmake .. -DCMAKE_BUILD_TYPE=Release
emmake make -j4

# Output will be in build_web/bin/
# Copy the .html, .js, .wasm files to your GitHub Pages directory
```

## Testing

### Local Testing
```bash
# Serve with Python
cd build_web/bin
python3 -m http.server 8000

# Visit http://localhost:8000/index.html
```

### Known Issues & Workarounds

1. **WebGL Context Loss**: Browser may lose WebGL context on tab switch
   - Solution: Implement `webglcontextlost` and `webglcontextrestored` event handlers

2. **Large Assets**: WASM file + preloaded assets can be large
   - Solution: Consider separating asset loading into chunks or using streaming

3. **Touch Input**: Browsers on mobile don't have pointer lock
   - Solution: Add touch controls as alternative to mouse camera

4. **Memory**: WASM runs in a single linear memory space
   - Monitor with `-sWARN_ON_UNDEFINED_SYMBOLS=1`

## Timeline

- [x] Refactored main loop for Emscripten
- [ ] Test shader compatibility with WebGL 2.0
- [ ] Update asset loading paths
- [ ] Disable/modify GPU timing queries
- [ ] Create/update HTML template
- [ ] Update GitHub Actions workflow
- [ ] Test on multiple browsers
- [ ] Deploy to GitHub Pages

## Resources

- [Emscripten Docs](https://emscripten.org/docs/)
- [Emscripten CMake](https://emscripten.org/docs/getting_started/cmake/index.html)
- [WebGL 2.0 Spec](https://www.khronos.org/webgl/wiki/WebGL_2.0)
- [GLFW for Emscripten](https://github.com/emscripten-core/emscripten/wiki/webglcontextlost-and-webglcontextrestored)

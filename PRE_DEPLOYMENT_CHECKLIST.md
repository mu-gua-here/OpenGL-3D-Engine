# Final Pre-Deployment Checklist

Use this checklist before your first `git push` to deploy to GitHub Pages.

## Code Quality

- [x] Native build compiles without errors
  ```bash
  cmake --build build --config Release
  ```
  Expected: "Built target OpenGL-3D-Engine"

- [x] No compilation warnings (optional, but cleaner)

- [x] All shader files are in `res/shaders/`
  - [x] pbr.vs, pbr.fs
  - [x] shadow.vs, shadow.fs
  - [x] skybox.vs, skybox.fs
  - [x] unlit.vs, unlit.fs

- [x] Asset files exist
  - [x] `res/scene_models/` (at least one model)
  - [x] `res/skyboxes/` (at least one skybox)
  - [x] `res/shaders/` (all shader files)

## Platform Configuration

- [ ] **main.cpp** - Verify optimizations:
  - [ ] V-Sync buttons wrapped in `#ifndef __EMSCRIPTEN__` (line ~981)
  - [ ] Shadow resolution conditional: 1024 on web, 4096 native (line ~169)
  - [ ] GPU queries wrapped: `#ifndef __EMSCRIPTEN__` (line ~971)
  - [ ] Loading screen shows during init (line ~825)

- [ ] **shader_loading.cpp** - Verify version injection:
  - [ ] Injects `#version 300 es` on web (line ~34)
  - [ ] Injects `#version 330 core` on native (line ~36)
  - [ ] Adds `precision mediump float;` on web (line ~34)

- [ ] **CMakeLists.txt** - Verify Emscripten block:
  - [ ] `if(EMSCRIPTEN)` block exists
  - [ ] Links against GLFW port (`-sUSE_GLFW=3`)
  - [ ] Links against WebGL2 (`-sUSE_WEBGL2=1`)
  - [ ] Memory configured (`-sALLOW_MEMORY_GROWTH=1`)

## GitHub Configuration

- [x] Repository has GitHub Pages enabled
  - Go to Settings → Pages
  - Source: Deploy from a branch
  - Branch: `gh-pages`

- [ ] GitHub Actions workflow exists
  - Check `.github/workflows/itchio_deploy.yml`
  - Uses `emcmake` and `emmake` commands
  - Uploads to `build/bin/Release/`

- [x] HTML shell template exists
  - Check `shell_minimal.html`
  - Contains `{{{ SCRIPT }}}` placeholder
  - No modifications needed

## Optional Validation Tests

- [x] **Test native build locally**
  ```bash
  ./build/bin/Release/OpenGL-3D-Engine
  ```
  Should show 3D scene, FPS counter, GPU timings.

- [ ] **Test web build locally** (optional, good to do first time)
  ```bash
  source ~/emsdk/emsdk_env.sh
  cd build
  emcmake cmake .. -DCMAKE_BUILD_TYPE=Release
  emmake make -j$(nproc)
  cd bin/Release
  python3 -m http.server 8000
  # Open http://localhost:8000/OpenGL-3D-Engine.html
  ```
  Should show loading screen, then 3D scene. No console errors.

## Documentation

- [ ] README.md is clear and up-to-date
- [ ] You've read [QUICK_START_WEB.md](QUICK_START_WEB.md)
- [ ] Optional: You've reviewed [WEB_OPTIMIZATION.md](WEB_OPTIMIZATION.md)

## Final Deployment

- [ ] All changes committed
  ```bash
  git status  # Should be clean
  ```

- [ ] Ready to push
  ```bash
  git push origin main
  ```

- [ ] GitHub Actions triggered
  - Check Actions tab on GitHub
  - Wait for ✅ "Deploy Engine to GitHub Pages"
  - Should take 2-3 minutes

- [ ] App is live
  - Visit: `https://<username>.github.io/<repo-name>`
  - Should see your 3D engine running
  - Check browser console (F12) for any errors

## Post-Deployment Validation

After pushing, verify:

- [ ] GitHub Actions workflow completed successfully
  - No red X on the workflow
  - All steps showing ✅

- [ ] GitHub Pages site is active
  - Go to Settings → Pages
  - Shows: "Your site is live at https://..."

- [ ] Web app loads
  - Loading screen appears
  - Engine renders after loading
  - No 404 errors in Network tab

- [ ] Controls work
  - Click to unlock cursor
  - WASD to move camera
  - Mouse to look around
  - ESC to release cursor

- [ ] Stats display correctly
  - FPS shows
  - Triangle count shows
  - Says "(GPU timing disabled on Web)"
  - No V-Sync buttons visible

## Troubleshooting

If something fails:

1. **Native build won't compile**
   - Check CMakeLists.txt syntax
   - Verify all source files exist
   - Try: `rm -rf build && mkdir build && cd build && cmake .. && make`

2. **GitHub Actions fails**
   - Check the workflow run logs (Actions tab)
   - Verify CMakeLists.txt EMSCRIPTEN block
   - Common issue: Wrong file paths

3. **Web app shows blank screen**
   - Open DevTools (F12)
   - Check Console for errors
   - Check Network tab for 404s
   - Make sure `res/` folder exists with assets

4. **Assets missing**
   - Assets must be in `res/shaders/`, `res/scene_models/`, `res/skyboxes/`
   - Emscripten auto-preloads them from `--preload-file` in CMakeLists.txt
   - Check build output for preload messages

## Success Criteria

You're done when:

✅ `git push origin main` succeeds  
✅ GitHub Actions workflow completes (✅ status)  
✅ GitHub Pages shows your app at `https://<username>.github.io/<repo-name>`  
✅ Engine loads and displays 3D scene  
✅ No console errors  
✅ Stats display correctly (no GPU timing on web)  

---

## Support Resources

- [QUICK_START_WEB.md](QUICK_START_WEB.md) - 3-step deployment guide
- [DEPLOYMENT.md](DEPLOYMENT.md) - Detailed deployment steps
- [WEB_OPTIMIZATION.md](WEB_OPTIMIZATION.md) - Technical reference
- [OPTIMIZATION_CHECKLIST.md](OPTIMIZATION_CHECKLIST.md) - What's been optimized

---

**When ready, run:**
```bash
git push origin main
```

**Then visit:**
```
https://<username>.github.io/<repo-name>
```

**Enjoy your web-deployed 3D engine!** 🚀

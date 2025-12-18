# Deployment Guide - GitHub Pages & itch.io

Quick reference for deploying your WebGL engine to the web.

## GitHub Pages Automatic Deployment

### Setup (One-time)

1. **Enable GitHub Pages in Settings**
   - Repository → Settings → Pages
   - Source: Deploy from a branch
   - Branch: `gh-pages` (auto-created by Actions)
   - Directory: `/ (root)`

2. **Workflow Already Configured**
   - `.github/workflows/itchio_deploy.yml` handles everything
   - Triggers on push to `main` branch
   - Auto-builds WebAssembly and deploys

### Deploy

```bash
git add .
git commit -m "Deploy to GitHub Pages"
git push origin main
```

**Result**: Your app is live at `https://<username>.github.io/<repo-name>`

---

## Manual Web Build (Local Testing)

If you need to test the web build locally:

```bash
# 1. Install Emscripten (first time only)
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest

# 2. Build
source ./emsdk_env.sh
cd /path/to/engine
mkdir -p build
cd build
emcmake cmake .. -DCMAKE_BUILD_TYPE=Release
emmake make -j$(nproc)

# 3. Serve locally
cd bin/Release
python3 -m http.server 8000

# 4. Open browser
# Visit http://localhost:8000/OpenGL-3D-Engine.html
```

---

## itch.io Deployment

### Option 1: Manual Upload

1. **Create itch.io Project**
   - Sign in to itch.io
   - Create new game project
   - Set as "HTML" game

2. **Build and Upload**
   ```bash
   # Build web version locally
   cd build/bin/Release
   
   # Create upload package
   zip -r OpenGL-3D-Engine.zip OpenGL-3D-Engine.html OpenGL-3D-Engine.js OpenGL-3D-Engine.wasm *.wasm
   ```

3. **Upload via Web Interface**
   - Game → Edit → Uploads
   - Choose "This file will be played in the browser"
   - Upload ZIP file

### Option 2: Automated with Butler CLI

```bash
# Install butler
npm install -g butler

# Set API key
export BUTLER_API_KEY=your_api_key_here

# Build
cd build/bin/Release

# Push to itch.io
butler push . username/game-name:web --userversion 1.0.0
```

---

## Troubleshooting

### White/Blank Screen
- Check browser console (F12) for errors
- Verify assets loaded: Network tab in DevTools
- Test locally first to isolate build vs deployment issues

### Assets Not Loading
- Ensure `--preload-file` includes correct paths in CMakeLists.txt
- Check that asset files exist in `res/` directory
- Browser console will show 404 errors for missing files

### Performance Issues
- Reduce shadow map resolution (already at 1024x1024 for web)
- Disable GPU timing UI (already disabled on web)
- Profile in Chrome DevTools: Performance tab

### Memory Errors
- WASM heap capped at 1GB (configurable in CMakeLists.txt)
- If needed, set `sALLOW_MEMORY_GROWTH=1` (already enabled)
- Monitor memory: Open DevTools → Memory tab

---

## Performance Tips

1. **First Load**: ~20-30 seconds (includes shader compilation)
2. **Subsequent Loads**: ~3-5 seconds (browser cache)
3. **FPS Target**: 30-60 (varies by device)

### For Users
- Chrome/Chromium: Best performance
- Firefox: Good performance
- Safari: Works but may be slower
- Edge: Similar to Chrome

### For Developers
- Use Chrome DevTools Performance tab to profile
- Check "Reduce motion" preference for smooth animations
- Test on different devices/bandwidth

---

## Build Artifacts

After building, you'll have:

```
build/bin/Release/
├── OpenGL-3D-Engine.html      # Entry point (open in browser)
├── OpenGL-3D-Engine.js        # JavaScript glue code
├── OpenGL-3D-Engine.wasm      # WebAssembly binary
└── OpenGL-3D-Engine.data      # Preloaded asset files
```

**Important**: Upload ALL files together for web deployment.

---

## GitHub Actions Workflow

The automated workflow:

1. Checks out code
2. Installs Emscripten SDK
3. Runs `emcmake cmake ..`
4. Runs `emmake make`
5. Uploads artifacts to GitHub Pages

**View Status**: GitHub → Actions tab → itchio_deploy workflow

---

## Custom Domain (GitHub Pages)

To use your own domain:

1. Add `CNAME` file to repository root with your domain name
2. Configure DNS to point to GitHub Pages
3. More info: [Custom domain docs](https://docs.github.com/en/pages/configuring-a-custom-domain-for-your-github-pages-site)

---

## FAQ

**Q: Can I deploy to Netlify/Vercel?**  
A: Yes! Follow the same steps but configure the workflow for those services instead.

**Q: How do I update after deployment?**  
A: Push to `main` branch → GitHub Actions rebuilds and redeploys automatically.

**Q: Can I password-protect the web build?**  
A: Yes, but requires additional setup with hosting provider (beyond scope here).

**Q: How large is the WASM file?**  
A: Typically 10-50MB depending on linked libraries (stripped in Release mode).

---

For more info on Emscripten deployment, see [WEB_OPTIMIZATION.md](WEB_OPTIMIZATION.md).

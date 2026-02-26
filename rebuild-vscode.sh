# Rebuild script for VS code development
cd ~/Desktop/OpenGL\ 3D\ Engine/

# Recreate build directory to reset build
rm -rf build-vscode
mkdir build-vscode
# Configure with Interprocedural Optimization (LTO) and Native CPU tuning
cmake -B build-vscode -G "Ninja Multi-Config" \
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON \
    -DCMAKE_CXX_FLAGS="-march=native"

# Build both Debug and Release configurations
cmake --build build-vscode --config Release --parallel
cmake --build build-vscode --config Debug --parallel
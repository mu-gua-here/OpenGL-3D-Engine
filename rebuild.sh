# Navigate to project root
cd ~/Desktop/OpenGL\ 3D\ Engine/

# Recreate build directory to reset build
rm -rf build
mkdir build
# Configure with Interprocedural Optimization (LTO) and Native CPU tuning
cmake -B build -G "Ninja Multi-Config" \
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON \
    -DCMAKE_CXX_FLAGS="-march=native"

# Build both Debug and Release configurations
cmake --build build --config Release --parallel
cmake --build build --config Debug --parallel
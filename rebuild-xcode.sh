# Rebuild script in case of developing in Xcode
rm -rf build-xcode
cmake -S . -B build-xcode -G Xcode -DCMAKE_BUILD_TYPE=Debug
cmake --build build-xcode -j
open build-xcode/*.xcodeproj
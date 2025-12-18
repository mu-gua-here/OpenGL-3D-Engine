#pragma once

#include <glm/glm.hpp>

class Color {
public:
    float r, g, b, a;
    
    Color() : r(0.0f), g(0.0f), b(0.0f), a(1.0f) {}
    Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}
    
    Color operator*(float scalar) const {
        return Color(r * scalar, g * scalar, b * scalar, a);
    }
    
    glm::vec4 toVec4() const { return glm::vec4(r, g, b, a); }
};

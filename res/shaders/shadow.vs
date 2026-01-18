layout(location = 0) in vec3 aPos;
layout(location = 2) in vec2 aTexCoords;
layout(location = 6) in mat4 instanceMatrix;

out vec2 TexCoord;

uniform mat4 lightSpaceMatrix;

void main() {
    TexCoord = aTexCoords;
    gl_Position = lightSpaceMatrix * instanceMatrix * vec4(aPos, 1.0);
}
layout (location = 0) in vec3 aPos;
layout (location = 6) in mat4 instanceModel;  // Instance matrix at locations 6-9

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    // Always use the instance model (it contains position, rotation, scale)
    gl_Position = projection * view * instanceModel * vec4(aPos, 1.0);
}

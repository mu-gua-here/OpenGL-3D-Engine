out vec4 FragColor;

uniform vec3 emissiveColor;
uniform float emissiveIntensity;

void main() {
    FragColor = vec4(emissiveColor * emissiveIntensity, 1.0);
}

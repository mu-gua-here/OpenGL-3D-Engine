out vec4 FragColor;

uniform vec3 emissiveColor;
uniform float emissiveIntensity;

void main() {
    vec3 finalColor = emissiveColor * emissiveIntensity;
    FragColor = vec4(finalColor, 1.0);
}

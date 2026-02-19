out vec4 FragColor;
in vec3 TexCoords;
uniform samplerCube skybox;

// Fog parameters
uniform bool enableFog;
uniform vec3 fogColor;

void main() {
    vec4 skyboxColor = texture(skybox, TexCoords);
    
    // Apply fog to skybox - makes it blend with fog color in distance
    // This effectively hides the skybox behind the fog
    if (enableFog) {
        // Skybox is at the far plane, so apply strong fog
        skyboxColor.rgb = mix(fogColor, skyboxColor.rgb, 0.3);  // 70% fog, 30% skybox color
    }
    
    FragColor = skyboxColor;
}

in vec2 texCoord;

out vec4 FragColor;

uniform sampler2D sceneTexture;
uniform sampler2D bloomTexture;
uniform float bloomIntensity;

void main() {
    vec4 sceneColor = texture(sceneTexture, texCoord);
    vec4 bloomColor = texture(bloomTexture, texCoord);
    
    // Add bloom on top of scene with intensity control
    vec3 finalColor = sceneColor.rgb + bloomColor.rgb * bloomIntensity;
    
    // Clamp to prevent overexposure in LDR
    finalColor = clamp(finalColor, vec3(0.0), vec3(1.0));
    
    FragColor = vec4(finalColor, sceneColor.a);
}

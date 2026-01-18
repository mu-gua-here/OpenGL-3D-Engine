in vec2 TexCoord;
uniform sampler2D albedoMap;
uniform bool hasAlbedoMap;

void main() {
    // Only test alpha for transparency
    if (hasAlbedoMap) {
        float alpha = texture(albedoMap, TexCoord).a;
        if (alpha < 0.5) discard;
    }
    // Depth is written automatically - no color output needed
}
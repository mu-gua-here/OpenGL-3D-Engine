in vec2 texCoord;

out vec4 FragColor;

uniform sampler2D colorTexture;
uniform float threshold;

void main() {
    vec4 color = texture(colorTexture, texCoord);
    float brightness = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    
    float bloomAmount = smoothstep(threshold - 0.15, threshold + 0.15, brightness);
    
    // Apply cubic ease-out curve for smoother bloom transition
    bloomAmount = bloomAmount * bloomAmount * (3.0 - 2.0 * bloomAmount);
    
    if (bloomAmount > 0.01) {
        // Higher-order modulation for more dramatic bloom in very bright areas
        float bloomPower = bloomAmount * (1.0 + bloomAmount * 0.5);
        FragColor = vec4(color.rgb * bloomPower, 1.0);
    } else {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}

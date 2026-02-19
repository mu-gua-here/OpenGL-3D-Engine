in vec2 texCoord;

out vec4 FragColor;

uniform sampler2D bloomTexture;
uniform float texelSizeX;

void main() {
    vec2 uv = texCoord;
    vec4 color = texture(bloomTexture, uv) * 0.2270270270;
    
    color += texture(bloomTexture, uv + vec2(texelSizeX * 1.3846153846, 0.0)) * 0.3162162162;
    color += texture(bloomTexture, uv - vec2(texelSizeX * 1.3846153846, 0.0)) * 0.3162162162;
    color += texture(bloomTexture, uv + vec2(texelSizeX * 3.2307692308, 0.0)) * 0.0702702703;
    color += texture(bloomTexture, uv - vec2(texelSizeX * 3.2307692308, 0.0)) * 0.0702702703;
    
    FragColor = color;
}

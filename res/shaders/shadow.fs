in vec2 TexCoord;

uniform sampler2D u_texture;

void main() {
    vec4 texColor = texture(u_texture, TexCoord);
    if (texColor.a < 0.5) {
        discard;
    }
    // Depth automatically written to shadow map
}
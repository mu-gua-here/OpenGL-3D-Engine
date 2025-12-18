in vec2 TexCoord;

uniform sampler2D u_texture;

void main() {
    vec4 texColor = texture(u_texture, TexCoord);
    if (texColor.a < 0.5) discard; // Prevents the fragment from writing to the depth buffer
    // Depth is automatically written
}

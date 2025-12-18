layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aNormal;
layout (location = 4) in vec3 aTangent;
layout (location = 5) in vec3 aBitangent;

out vec4 vertexColor;
out vec2 TexCoord;
out vec3 FragPos;
out vec3 Normal;
out vec4 FragPosLightSpace;
out mat3 TBN;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;
uniform mat4 lightSpaceMatrix;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    
    // Transform normal and tangent to world space
    vec3 N = normalize(normalMatrix * aNormal);
    vec3 T = normalize(normalMatrix * aTangent);

    // Re-orthogonalize T with respect to N using Gram-Schmidt process
    T = normalize(T - dot(T, N) * N);

    // Compute bitangent from the corrected normal and tangent
    vec3 B = cross(N, T);

    // Create TBN matrix for tangent space calculations
    TBN = mat3(T, B, N);
    
    Normal = normalMatrix * aNormal;
    TexCoord = aTexCoords;
    vertexColor = aColor;
    
    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);
    
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
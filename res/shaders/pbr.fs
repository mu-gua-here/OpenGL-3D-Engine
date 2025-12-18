in vec4 vertexColor;
in vec2 TexCoord;
in vec3 FragPos;
in vec3 Normal;
in vec4 FragPosLightSpace;
in mat3 TBN;

out vec4 FragColor;

// Samplers
uniform sampler2D albedoMap;
uniform sampler2D normalMap;
uniform sampler2D ormMap;
uniform sampler2D emissiveMap;
uniform sampler2DShadow shadowMap;

// Material scalar properties
uniform vec3 baseColor;
uniform float metallic;
uniform float roughness;
uniform float ao;
uniform vec3 emissive;
uniform float heightScale;

uniform bool hasAlbedoMap;
uniform bool hasNormalMap;
uniform bool hasEmissiveMap;
uniform bool hasORMMap;
uniform bool hasHeightMap;

// Lighting
#define MAX_LIGHTS 8
uniform int lightCount;
uniform vec3 viewPos;
uniform int shadowLightIndex;
uniform mat4 lightSpaceMatrix;

uniform vec3 lightPositions[MAX_LIGHTS];
uniform vec3 lightColors[MAX_LIGHTS];
uniform float lightIntensities[MAX_LIGHTS];
uniform vec3 lightDirections[MAX_LIGHTS];
uniform float lightInnerCutoffs[MAX_LIGHTS];
uniform float lightOuterCutoffs[MAX_LIGHTS];
uniform float lightTypes[MAX_LIGHTS];

const float PI = 3.14159265359;

// PARALLAX OCCLUSION MAPPING
vec2 parallaxMapping(vec2 texCoords, vec3 viewDir) { 
    const float minLayers = 8;
    const float maxLayers = 64;
    float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 1.0), viewDir)));
    // Calculate the size of each layer
    float layerDepth = 1.0 / numLayers;
    float currentLayerDepth = 0.0;
    // The amount to shift the texture coordinates per layer
    vec2 P = viewDir.xy / viewDir.z * heightScale;
    vec2 deltaTexCoords = P / numLayers;
  
    vec2 currentTexCoords = texCoords;
    float currentDepthMapValue = texture(ormMap, currentTexCoords).a;
      
    while (currentLayerDepth < currentDepthMapValue) {
        currentTexCoords -= deltaTexCoords;
        currentDepthMapValue = texture(ormMap, currentTexCoords).a;  
        currentLayerDepth += layerDepth;  
    }
    
    vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

    float afterDepth = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = texture(ormMap, prevTexCoords).a - currentLayerDepth + layerDepth;
 
    // Linear interpolation of texture coordinates
    float weight = afterDepth / (afterDepth - beforeDepth);
    vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

    return finalTexCoords;
}

// SHADOW MAPPING
float calcShadow(vec4 fragLight, vec3 N, vec3 L) {
    if (shadowLightIndex < 0) return 0.0;

    // Apply normal offset before projection to prevent acne
    vec3 offsetPos = FragPos + N * 0.1;

    vec4 offsetLight = lightSpaceMatrix * vec4(offsetPos, 1.0);
    
    vec3 proj = offsetLight.xyz / offsetLight.w;
    proj = proj * 0.5 + 0.5;

    if (proj.z > 1.0) return 0.0;

    // Minimal bias needed since we use normal offset
    float cosTheta = max(dot(N, L), 0.0);
    float bias = 0.0001 * (1.0 - cosTheta);

    // Poisson disk sampling offsets
    vec2 poissonDisk[16] = vec2[](
        vec2(-0.94201624, -0.39906216),
        vec2(0.94558609, -0.76890725),
        vec2(-0.094184101, -0.92938870),
        vec2(0.34495938, 0.29387760),
        vec2(-0.91588581, 0.45771432),
        vec2(-0.81544232, -0.87912464),
        vec2(-0.38277543, 0.27676845),
        vec2(0.97484398, 0.75648379),
        vec2(0.44323325, -0.97511554),
        vec2(0.53742981, -0.47373420),
        vec2(-0.26496911, -0.41893023),
        vec2(0.79197514, 0.19090188),
        vec2(-0.24188840, 0.99706507),
        vec2(-0.81409955, 0.91437590),
        vec2(0.19984126, 0.78641367),
        vec2(0.14383161, -0.14100790)
    );

    // PCF sampling with Poisson disk distribution
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    
    // Adjust the multiplier to control shadow softness (higher = softer)
    float spread = 2.0;
    
    for (int i = 0; i < 16; ++i) {
        vec2 offset = poissonDisk[i] * texelSize * spread;
        float shadowSample = texture(shadowMap, vec3(proj.xy + offset, proj.z - bias));
        shadow += shadowSample; // Returns 0.0 (in shadow) or 1.0 (lit)
    }
    shadow /= 16.0;

    // Fade shadows at edges
    vec2 border = min(proj.xy, 1.0 - proj.xy);
    float fade = smoothstep(0.0, 0.1, min(border.x, border.y));
    
    // Invert because sampler2DShadow returns 1.0 for lit, 0.0 for shadow
    return mix(0.0, 1.0 - shadow, fade);
}

// PBR FUNCTIONS
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float D_GGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    denom = PI * denom * denom;
    
    return a2 / max(denom, 0.0001);
}

float G_SchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    
    float denom = NdotV * (1.0 - k) + k;
    
    return NdotV / max(denom, 0.0001);
}

float G_Smith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = G_SchlickGGX(NdotV, roughness);
    float ggx1 = G_SchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

// MAIN
void main() {
    // Calculate view direction in tangent space for POM
    vec3 Vworld = normalize(viewPos - FragPos);
    vec2 uv = TexCoord;
    
    if (hasHeightMap) {
        vec3 Vts = normalize(transpose(TBN) * Vworld);
        if (abs(Vts.z) < 0.001) Vts.z = 0.001;
        uv = parallaxMapping(TexCoord, Vts);
    }
    
    // Sample material properties
    vec4 albedoSample = hasAlbedoMap ? texture(albedoMap, uv) : vec4(baseColor, 1.0);
    if (albedoSample.a < 0.5) discard;
    
    vec3 albedo = albedoSample.rgb;
    
    // Sample or use scalar values for ORM
    float aoValue = ao;
    float roughValue = roughness;
    float metalValue = metallic;
    
    if (hasORMMap) {
        vec3 ormSample = texture(ormMap, uv).rgb;
        aoValue = ormSample.r;
        roughValue = ormSample.g;
        metalValue = ormSample.b;
    }
    
    // Clamp values to valid ranges
    roughValue = clamp(roughValue, 0.04, 1.0);  // Minimum roughness to avoid artifacts
    metalValue = clamp(metalValue, 0.0, 1.0);
    aoValue = clamp(aoValue, 0.0, 1.0);
    
    vec3 emissiveCol = hasEmissiveMap ? texture(emissiveMap, uv).rgb : emissive;
    
    // Calculate normal
    vec3 N = normalize(Normal);
    if (hasNormalMap) {
        vec3 nt = texture(normalMap, uv).rgb * 2.0 - 1.0;
        N = normalize(TBN * nt);
    }
    
    // Flip normal if facing away
    if (!gl_FrontFacing) N = -N;
    
    vec3 V = normalize(Vworld);
    
    // Calculate F0 (base reflectivity)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metalValue);
    
    // Lighting calculation
    vec3 Lo = vec3(0.0);
    
    for (int i = 0; i < lightCount && i < MAX_LIGHTS; ++i) {
        vec3 L;
        float attenuation = 1.0;
        
        // Calculate light direction and attenuation
        if (lightTypes[i] == 0.0) {
            // Directional light
            L = normalize(-lightDirections[i]);
        } else {
            // Point or spot light
            L = normalize(lightPositions[i] - FragPos);
            float distance = length(lightPositions[i] - FragPos);
            attenuation = 1.0 / (distance * distance);
            
            // Spotlight attenuation
            if (lightTypes[i] == 2.0) {
                float theta = dot(L, normalize(-lightDirections[i]));
                float epsilon = lightInnerCutoffs[i] - lightOuterCutoffs[i];
                float intensity = clamp((theta - lightOuterCutoffs[i]) / epsilon, 0.0, 1.0);
                attenuation *= intensity;
            }
        }
        
        vec3 H = normalize(V + L);
        vec3 radiance = lightColors[i] * lightIntensities[i] * attenuation;
        
        // Cook-Torrance BRDF
        float NDF = D_GGX(N, H, roughValue);
        float G = G_Smith(N, V, L, roughValue);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metalValue;
        
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;
        
        // Shadow calculation
        float shadow = (i == shadowLightIndex) ? calcShadow(FragPosLightSpace, N, L) : 0.0;
        
        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL * (1.0 - shadow);
    }
    
    // Ambient lighting with AO
    vec3 ambient = vec3(0.3) * albedo * aoValue;
    vec3 color = ambient + Lo + emissiveCol;
    
    // Tone mapping (Reinhard)
    color = color / (color + vec3(1.0));
    
    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}

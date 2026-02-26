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

// Fog parameters
uniform bool enableFog;
uniform vec3 fogColor;
uniform float fogStart;
uniform float fogEnd;

// Ambient lighting (replaces skybox IBL)
uniform vec3 ambientColor;
uniform float ambientIntensity;

// Render detail level (0 = low, 1 = medium, 2 = high)
uniform int renderDetailLevel;

const float PI = 3.14159265359;

// PARALLAX OCCLUSION MAPPING
vec2 parallaxMapping(vec2 texCoords, vec3 viewDir) { 
    const float minLayers = 8.0;
    const float maxLayers = 64.0;
    float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 1.0), viewDir)));
    float layerDepth = 1.0 / numLayers;
    float currentLayerDepth = 0.0;
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
    float weight = afterDepth / (afterDepth - beforeDepth);
    vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

    return finalTexCoords;
}

// IGN NOISE FUNCTION
float InterleavedGradientNoise(vec2 uv) {
    vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(uv, magic.xy)));
}

// SHADOW MAPPING
float calcShadow(vec4 fragLight, vec3 N, vec3 L, vec3 lightColor) {
    if (shadowLightIndex < 0) return 1.0;

    // Perspective divide
    vec3 proj = fragLight.xyz / fragLight.w;
    
    // Transform to [0,1] range
    proj = proj * 0.5 + 0.5;

    // Check if fragment is outside shadow map bounds
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) {
        return 1.0;  // No shadow outside frustum
    }

    // Adaptive bias: smaller for distant surfaces (less acne), larger for close surfaces (less peter-panning)
    float cosTheta = max(dot(N, L), 0.0);
    float bias = mix(0.0002, 0.002, cosTheta);  // Distance-adaptive bias
    
    // Get texel size for PCF
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    
    // PCF with rotated Poisson sampling
    float noise = InterleavedGradientNoise(gl_FragCoord.xy);
    float angle = noise * 6.283185;
    float s = sin(angle);
    float c = cos(angle);
    mat2 rotation = mat2(c, -s, s, c);
    
    vec2 poissonDisk[8] = vec2[](
        vec2(-0.94201624, -0.39906216),
        vec2(0.94558609, -0.76890725),
        vec2(-0.094184101, -0.92938870),
        vec2(0.34495938, 0.29387760),
        vec2(-0.91588581, 0.45771432),
        vec2(-0.81544232, -0.87912464),
        vec2(-0.38277543, 0.27676845),
        vec2(0.97484398, 0.75648379)
    );
    
    float shadow = 0.0;
    // Distance-aware filter: smaller radius = sharper shadows, more aliasing
    float distanceFromLight = length(FragPosLightSpace.xyz);
    float filterRadius = 0.8 + distanceFromLight * 0.02;  // Increase slightly with distance to hide aliasing
    
    for (int i = 0; i < 8; i++) {
        vec2 offset = rotation * poissonDisk[i] * texelSize * filterRadius;
        shadow += texture(shadowMap, vec3(proj.xy + offset, proj.z - bias));
    }
    shadow /= 8.0;
    
    // Smooth fade at shadow map edges
    vec2 edgeFade = smoothstep(0.0, 0.05, proj.xy) * (1.0 - smoothstep(0.95, 1.0, proj.xy));
    float fade = edgeFade.x * edgeFade.y;
    
    // Brightness-aware shadow softening: shadows are less harsh in bright areas
    float lightBrightness = dot(lightColor, vec3(0.299, 0.587, 0.114));
    float shadowSoftness = mix(0.6, 0.9, lightBrightness);  // Brighter lights = softer shadows
    shadow = mix(shadow, 1.0, 1.0 - shadowSoftness);  // Blend towards 1.0 (less shadow)
    
    return mix(1.0, shadow, fade);
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
    vec2 uv = TexCoord;
    
    // Alpha test first before any other sampling
    if (hasAlbedoMap) {
        float alpha = texture(albedoMap, uv).a;
        if (alpha < 0.5) discard;
    }
    
    vec3 Vworld = normalize(viewPos - FragPos);
    float distToCam = length(viewPos - FragPos);
    
    // Determine detail blend: 1.0 = full detail, 0.0 = no detail (smooth transition)
    float detailBlend = (renderDetailLevel == 0) ? 0.0 : 1.0 - smoothstep(30.0, 50.0, distToCam);
    
    // Skip expensive parallax mapping when far
    bool useParallax = (detailBlend > 0.7) && hasHeightMap && heightScale > 0.001 && distToCam < 20.0;
    
    if (useParallax) {
        vec3 Vts = normalize(transpose(TBN) * Vworld);
        if (abs(Vts.z) > 0.001) {
            uv = parallaxMapping(TexCoord, Vts);
        }
    }
    
    // Sample base textures
    vec4 albedoSample = hasAlbedoMap ? texture(albedoMap, uv) : vec4(baseColor, 1.0);
    vec3 albedo = albedoSample.rgb;
    
    vec3 N = normalize(Normal);
    if (hasNormalMap && detailBlend > 0.7) {
        vec3 nt = texture(normalMap, uv).rgb * 2.0 - 1.0;
        N = normalize(TBN * nt);
    }
    
    if (!gl_FrontFacing) N = -N;
    
    // Sample material properties
    vec3 ormSample = (detailBlend > 0.5 && hasORMMap) ? texture(ormMap, uv).rgb : vec3(ao, roughness, metallic);
    float aoValue = ormSample.r;
    float roughValue = clamp(ormSample.g, 0.04, 1.0);
    float metalValue = clamp(ormSample.b, 0.0, 1.0);
    
    vec3 emissiveCol = (detailBlend > 0.7 && hasEmissiveMap) ? texture(emissiveMap, uv).rgb : emissive;
    
    vec3 V = normalize(Vworld);
    vec3 F0 = mix(vec3(0.04), albedo, metalValue);
    
    // Lighting calculation
    vec3 Lo = vec3(0.0);
    
    for (int i = 0; i < lightCount && i < MAX_LIGHTS; ++i) {
        vec3 L;
        float attenuation = 1.0;
        
        if (lightTypes[i] == 0.0) {
            L = normalize(-lightDirections[i]);
        } else {
            L = normalize(lightPositions[i] - FragPos);
            float distance = length(lightPositions[i] - FragPos);
            attenuation = 1.0 / (distance * distance);
            if (attenuation < 0.0001) continue;
            
            if (lightTypes[i] == 2.0) {
                float theta = dot(L, normalize(-lightDirections[i]));
                float epsilon = lightInnerCutoffs[i] - lightOuterCutoffs[i];
                float intensity = clamp((theta - lightOuterCutoffs[i]) / epsilon, 0.0, 1.0);
                attenuation *= intensity;
            }
        }
        
        vec3 H = normalize(V + L);
        vec3 radiance = lightColors[i] * lightIntensities[i] * attenuation;
        
        float NDF = D_GGX(N, H, roughValue);
        float G = G_Smith(N, V, L, roughValue);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metalValue;
        
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;
        
        float shadow = (i == shadowLightIndex) ? calcShadow(FragPosLightSpace, N, L, lightColors[i]) : 1.0;
        
        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL * shadow;
    }
    
    vec3 ambient = ambientColor * ambientIntensity * albedo * aoValue;
    vec3 color = ambient + Lo + emissiveCol;
    
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));
    
    // Apply fog effect
    if (enableFog) {
        float distance = length(viewPos - FragPos);
        if (distance > fogEnd) discard;  // Fully fogged out
        float fogFactor = clamp((fogEnd - distance) / (fogEnd - fogStart), 0.0, 1.0);
        color = mix(fogColor, color, fogFactor);
    }
    
    FragColor = vec4(color, 1.0);
}

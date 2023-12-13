#ifndef _LIGHTING_UTIL_HLSLI_
#define _LIGHTING_UTIL_HLSLI_

#include "Common.hlsli"

struct Material
{
    float4 Diffuse;
    float  Specular;
    float  Fresnel;
};

#define BLINN_FACTOR     128.0f
#define SPEC_MULTIPLIER   10.0f

// Learnopengl: https://learnopengl.com/Advanced-Lighting/Normal-Mapping 
float3 normal_sample_to_world_space(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
    float3 normalT = 2.0f * normalMapSample - 1.0f;

    float3 N = unitNormalW;
    float3 T = normalize(tangentW - dot(tangentW, N) * N);
    float3 B = cross(N, T);

    float3x3 TBN = float3x3(T, B, N);

    return mul(normalT, TBN);
}

// Wikipedia: https://en.wikipedia.org/wiki/Schlick%27s_approximation
float schlick_fresnel(float3 normal, float3 V, float fresnel)
{
    float cosTheta = saturate(dot(normal, V));
    float f0       = 1.0f - cosTheta;
    float r0       = (fresnel - 1.0f) / (fresnel + 1.0f);
    r0             = r0 * r0;

    return r0 + (1.0f - r0) * (f0 * f0 * f0 * f0 * f0);
}

float3 directional_light(float3 pointPos, float3 normal, Light light, Material m)
{
    float3 color = m.Diffuse.xyz * light.Strength * max(dot(normal, -light.Direction), 0.0f);
    
    float3 V = normalize(-1.0f * pointPos);
    float3 H = normalize(V - light.Direction);
        
    color += m.Specular.xxx * light.Strength * SPEC_MULTIPLIER *
             pow(max(dot(H, normal), 0.0f), BLINN_FACTOR) * schlick_fresnel(normal, V, m.Fresnel);
    
    return color;
}

// Iquilez: https://iquilezles.org/articles/fog/
float3 fog(float3 prevColor, float4 fogColor, float3 posW, float3 eyePos, float far)
{
    float  dist   = length(posW - eyePos);
    float  fogFac = 1.0f - exp(-pow(5.0f * saturate(dist / (far + 10.0f)), 1.8f));
    float3 fogCol = 0.80f * fogColor.rgb;
    return lerp(prevColor.rgb, fogColor.rgb, fogFac * fogColor.a);
}

#endif

#ifndef _SHADOW_HLSLI_
#define _SHADOW_HLSLI_

#include "Common.hlsli"

uint obtain_split(float z)
{
    uint res = 0;
    
    [unroll]
    for (uint i = 0; i < NUM_SHADOW_SPLIT; ++i)
    {
        if(z < Shadow_Data[i].SplitDistance)
        {
            res = i;
        }
    }

    return res;
}

float shadow(float4 pos, uint splitIndex)
{
    pos.xyz /= pos.w;
   
    const float depth = pos.z;

    uint w, h, mips;
    tShadowMaps[splitIndex].GetDimensions(0, w, h, mips);

    const float dw = 1.0f / (float) w;
    const float2 offsets[9] =
    {
        float2(-dw,  -dw), float2(0.0f,  -dw), float2(dw,  -dw),
        float2(-dw, 0.0f), float2(0.0f, 0.0f), float2(dw, 0.0f),
        float2(-dw,   dw), float2(0.0f,   dw), float2(dw,   dw)
    };
    
    float lit = 0.0f;
    [unroll]
    for (uint i = 0; i < 9; ++i)
    {
        lit += tShadowMaps[splitIndex].SampleCmpLevelZero(Sampler_Shadow, pos.xy + offsets[i], depth).r;
    }
    lit /= 9.0f;

    return lit;
}

#endif
#ifndef _COMPUTE_HNC_TILE_HLSL_
#define _COMPUTE_HNC_TILE_HLSL_

#include "CommonCompute.hlsli"
#include "Noise.hlsli"

struct TileColorTextureLayer
{
    uint TextureSlot;
    uint ColorLayer;
    float MinValue;
    float MaxValue;
};
cbuffer cbConstants : register(b0)
{
    float2 TerrainSize;
    float2 TileSize;
    float2 Min;
    float2 Max;
    float Scale;
    uint LayerCount;
    TileColorTextureLayer ColorTextureLayers[8];
}


RWTexture2D<half> tHeightmap : register(u0);
RWTexture2D<float4> tNormal : register(u1);
RWTexture2D<float4> tColorAlbedo : register(u2);
Texture2D tTextureMaps[8] : register(t0, space1);

SamplerState Sampler_PointWrap : register(s0);
SamplerState Sampler_AnisoWrap : register(s1);

[numthreads(TG_SIZE_X, TG_SIZE_Y, TG_SIZE_Z)]
void CS( uint3 DTid : SV_DispatchThreadID )
{
    float2 pos = DTid.xy;
    
    const float2 deltaTile = Max - Min;
    const float2 groupScale = deltaTile / TileSize;

    pos = ((pos * groupScale) + Min) / TerrainSize;

    float heightmapValue = fbm_terrain(pos);
    heightmapValue = heightmapValue * heightmapValue * heightmapValue;

    tHeightmap[DTid.xy] = heightmapValue;
    tNormal[DTid.xy] = float4(1.0, 1.0, 0.0, 1.0);
    float2 uv = pos * 1000.0f.xx;
    tColorAlbedo[DTid.xy] = tTextureMaps[5].SampleLevel(Sampler_AnisoWrap, uv, 1);
}

#endif
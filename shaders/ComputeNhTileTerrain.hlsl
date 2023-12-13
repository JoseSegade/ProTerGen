#ifndef _COMPUTE_Nh_TILE_TERRAIN_HLSL_
#define _COMPUTE_Nh_TILE_TERRAIN_HLSL_

#include "CommonCompute.hlsli"
#include "Noise.hlsli"

cbuffer cbConstants : register(b0)
{
    float2 Terrain_Size;
    float2 Terrain_TileSize;
    float2 Terrain_Min;
    float2 Terrain_Max;
    uint   Terrain_Mip;
}

cbuffer cbNoiseConstants : register(b1)
{
    float Noise_Amplitude;
    float Noise_Frecuency;
    float Noise_Gain;
    uint  Noise_Octaves;
    float Noise_Seed;
    float Noise_LayerWeight;
    uint  Noise_LayerIndex;
    float _pad;
}

RWTexture2D<float4> tOut : register(u0);

SamplerState Sampler_PointWrap : register(s0);
SamplerState Sampler_AnisoWrap : register(s1);

#ifdef _DEFAULT_

void get_slope_and_derivatives(float2 uv, float step, out float height, out float slope, out float2 derivatives)
{
    float2 minUVSample = float2(step, step);
    float2 maxUVSample = 1.0 - step;
    
    // UV coordinates start the bottom left corner.
    // H5 is the height of the current texture coordinate.
    //                                  uv(1, 1)
    // ----------------------------------
    // |          |          |          |
    // |    H1    |    H2    |    H3    |
    // |          |          |          |
    // ----------------------------------
    // |          |          |          |
    // |    H4    |    H5    |    H6    |
    // |          |          |          |
    // ----------------------------------
    // |          |          |          |
    // |    H7    |    H8    |    H9    |
    // |          |          |          |
    // ----------------------------------
    // uv(0, 0)
    // The aproximation of the partial derivative is the change in height between the 
    // sides of the square formed by the adjacent heights of the the current one.
    // It can be interpreted as an equally ponderated change in heights 
    // of the 2 dimensions of the map.
    FBM fbm =
    {
        Noise_Amplitude,
        Noise_Frecuency,
        Noise_Gain,
        Noise_Octaves
    };
    
    float2 texC1 = clamp(uv + float2(-step, step),  minUVSample, maxUVSample);
    float2 texC2 = clamp(uv + float2(0.0f, step),   minUVSample, maxUVSample);
    float2 texC3 = clamp(uv + float2(step, step),   minUVSample, maxUVSample);
    float2 texC4 = clamp(uv + float2(-step, 0.0f),  minUVSample, maxUVSample);
    float2 texC5 = clamp(uv + float2(0.0f, 0.0f),   minUVSample, maxUVSample);
    float2 texC6 = clamp(uv + float2(step, 0.0f),   minUVSample, maxUVSample);
    float2 texC7 = clamp(uv + float2(-step, -step), minUVSample, maxUVSample);
    float2 texC8 = clamp(uv + float2(0.0f, -step),  minUVSample, maxUVSample);
    float2 texC9 = clamp(uv + float2(step, -step),  minUVSample, maxUVSample);
        
    //float h1 = fbm_terrain(texC1);
    //float h2 = fbm_terrain(texC2);
    //float h3 = fbm_terrain(texC3);
    //float h4 = fbm_terrain(texC4);
    //float h5 = fbm_terrain(texC5);
    //float h6 = fbm_terrain(texC6);
    //float h7 = fbm_terrain(texC7);
    //float h8 = fbm_terrain(texC8);
    //float h9 = fbm_terrain(texC9);
    float h1 = fbm_terrain_params(texC1, fbm, Noise_Seed);
    float h2 = fbm_terrain_params(texC2, fbm, Noise_Seed);
    float h3 = fbm_terrain_params(texC3, fbm, Noise_Seed);
    float h4 = fbm_terrain_params(texC4, fbm, Noise_Seed);
    float h5 = fbm_terrain_params(texC5, fbm, Noise_Seed);
    float h6 = fbm_terrain_params(texC6, fbm, Noise_Seed);
    float h7 = fbm_terrain_params(texC7, fbm, Noise_Seed);
    float h8 = fbm_terrain_params(texC8, fbm, Noise_Seed);
    float h9 = fbm_terrain_params(texC9, fbm, Noise_Seed);

    h1 = h1 *h1 *h1 ;
    h2 = h2 *h2 *h2 ;    
    h3 = h3 *h3 *h3 ;    
    h4 = h4 *h4 *h4 ;    
    h5 = h5 *h5 *h5 ;    
    h6 = h6 *h6 *h6 ;    
    h7 = h7 *h7 *h7 ;    
    h8 = h8 *h8 *h8 ;    
    h9 = h9 *h9 *h9 ;    
    
    float hx = (h3 + h6 + h9 - h1 - h4 - h7) / (6.0f);
    float hy = (h1 + h2 + h3 - h7 - h8 - h9) / (6.0f);
    
    height = h5;
    derivatives = float2(-hx, -hy);

    float p = derivatives.x * derivatives.x + derivatives.y * derivatives.y;
    float g = sqrt(p);
    slope = degrees(atan(g)) / 90.0f;
}

[numthreads(TG_SIZE_X, TG_SIZE_Y, TG_SIZE_Z)]
void CS( uint3 DTid : SV_DispatchThreadID )
{
    float2 pos = DTid.xy;
    
    const float2 deltaTile = Terrain_Max - Terrain_Min;
    const float2 groupScale = deltaTile / Terrain_TileSize;

    pos = ((pos * groupScale) + Terrain_Min) / Terrain_Size;

    float h = 0.0f;
    float slope = 0.0f;
    float2 d = float2(0.0f, 0.0f);

    get_slope_and_derivatives(pos, 0.001, h, slope, d);

    tOut[DTid.xy] = (Noise_LayerWeight * float4(slope, d, h)) + (1.0f - Noise_LayerWeight) * tOut[DTid.xy];
}

#endif

#ifdef FBM_GRADIENT_NOISE

[numthreads(TG_SIZE_X, TG_SIZE_Y, TG_SIZE_Z)]
void CS( uint3 DTid : SV_DispatchThreadID )
{
    float2 pos = DTid.xy;
    
    const float2 deltaTile = Terrain_Max - Terrain_Min;
    const float2 groupScale = deltaTile / Terrain_TileSize;

    pos = ((pos * groupScale) + Terrain_Min) / Terrain_Size;

    const FBM fbm =
    {
        Noise_Amplitude,
        Noise_Frecuency,
        Noise_Gain,
        Noise_Octaves
    };
    //const float h = fbm_terrain_params(pos, fbm, Noise_Seed);
    const float h = fbm_terrain_params(pos, fbm, Noise_Seed);
    
    const float firstWeight = saturate(round((float) Noise_LayerIndex));
    const float prevH = firstWeight * tOut[DTid.xy].a;

    const float finalH = lerp(prevH, h, saturate(Noise_LayerWeight));

    tOut[DTid.xy] = float4(0.0f, 0.0f, 0.0f, finalH);
}

#endif

#ifdef FBM_VORONOI_NOISE

[numthreads(TG_SIZE_X, TG_SIZE_Y, TG_SIZE_Z)]
void CS( uint3 DTid : SV_DispatchThreadID )
{
    float2 pos = DTid.xy;
    
    const float2 deltaTile = Terrain_Max - Terrain_Min;
    const float2 groupScale = deltaTile / Terrain_TileSize;

    pos = ((pos * groupScale) + Terrain_Min) / Terrain_Size;

    const FBM fbm =
    {
        Noise_Amplitude,
        Noise_Frecuency,
        Noise_Gain,
        Noise_Octaves
    };
    const float h = fbm_terrain_params_voronoi(pos, fbm, Noise_Seed);
    
    const float firstWeight = saturate(round((float) Noise_LayerIndex));
    const float prevH = firstWeight * tOut[DTid.xy].a;

    const float finalH = lerp(prevH, h, saturate(Noise_LayerWeight));

    tOut[DTid.xy] = float4(0.0f, 0.0f, 0.0f, finalH);
}

#endif

#ifdef SLOPE_DERIVATIVES

[numthreads(TG_SIZE_X, TG_SIZE_Y, TG_SIZE_Z)]
void CS( uint3 DTid : SV_DispatchThreadID )
{
    float slope = 0.0f;

    const int2 minP = int2(0, 0);
    const int2 maxP = int2((int) Terrain_TileSize.x, (int) Terrain_TileSize.y);
    const int step = 1;
    const uint2 p1 = clamp((int2)DTid.xy + int2(-step,  step), minP, maxP);
    const uint2 p2 = clamp((int2)DTid.xy + int2(    0,  step), minP, maxP);
    const uint2 p3 = clamp((int2)DTid.xy + int2( step,  step), minP, maxP);
    const uint2 p4 = clamp((int2)DTid.xy + int2(-step,     0), minP, maxP);
    const uint2 p5 = clamp((int2)DTid.xy + int2(    0,     0), minP, maxP);
    const uint2 p6 = clamp((int2)DTid.xy + int2( step,     0), minP, maxP);
    const uint2 p7 = clamp((int2)DTid.xy + int2(-step, -step), minP, maxP);
    const uint2 p8 = clamp((int2)DTid.xy + int2(    0, -step), minP, maxP);
    const uint2 p9 = clamp((int2)DTid.xy + int2( step, -step), minP, maxP);

    const float h1 = tOut[p1].a;
    const float h2 = tOut[p2].a;
    const float h3 = tOut[p3].a;
    const float h4 = tOut[p4].a;
    const float h5 = tOut[p5].a;
    const float h6 = tOut[p6].a;
    const float h7 = tOut[p7].a;
    const float h8 = tOut[p8].a;
    const float h9 = tOut[p9].a;
    
    const float hx = (h3 + h6 + h9 - h1 - h4 - h7) / (6.0f);
    const float hy = (h1 + h2 + h3 - h7 - h8 - h9) / (6.0f);
    
    float2 di = float2(-hx, -hy) * (Terrain_TileSize / (1 << Terrain_Mip));

    float2 d = di * di;
    const float p = d.x * d.x + d.y * d.y;
    const float g = sqrt(p);
    slope = degrees(atan(g)) / 90.0f;

    tOut[DTid.xy] = float4(slope, di, tOut[p5].a);
}

#endif

#endif

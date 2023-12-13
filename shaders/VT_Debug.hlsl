#ifndef _VT_DEBUG_HLSL_
#define _VT_DEBUG_HLSL_

#include "Common.hlsli"

struct VertexIn
{
    float3 Pos : POSITION;
    float3 Normal : NORMAL;
    float2 TexC : TEXCOORD;
    float3 Tangent : TANGENT;
};

struct VertexOut
{
    float4 Pos : SV_POSITION;
    float2 TexC : TEXCOORD;
};

typedef VertexOut PixelIn;
typedef float4 PixelOut;

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0;
    float x = -vin.Pos.x;
    float y = vin.Pos.z;
    float z = vin.Pos.y;
    vout.Pos = float4(x, y, z, 1.0f);
    vout.TexC = float2(0.5 + x * 0.5, -(0.5 + y * 0.5));
    
    return vout;
}

PixelOut PS(PixelIn pin) : SV_TARGET
{
    float2 uv =  pin.TexC;
    
    int n = floor(uv.x * 2.0) % 2;
    int index = (n * VT_PageTableIndex) + ((1 - n) * VT_AtlasIndex);
    
    uv.x = fmod(2.0 * uv.x, 2.0);    
    
    float4 atlas_color = tTextureMaps[index].Sample(Sampler_PointWrap, uv);
        
    return float4(atlas_color.rgb * (n) + atlas_color.aaa * (1 - n), 0.8f);
}

#endif

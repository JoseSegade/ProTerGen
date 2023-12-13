#ifndef _VTFEEDBACK_HLSL_
#define _VTFEEDBACK_HLSL_

#include "Common.hlsli"
#include "VirtualTexture.hlsli"

struct VertexIn
{
    float3 Pos : POSITION;
    float3 Normal : NORMAL;
    float2 TexC : TEXCOORD;
    float3 Tangent : TANGENT;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

typedef VertexOut PixelIn;
typedef float4 PixelOut;

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f;
    
    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), Obj_TexTransform).xy;

    const float4 posW = mul(float4(vin.Pos + float3(0.0f, iEyePosW.y - 50.0f, 0.0f), 1.0f), Obj_World);
    vout.PosH = mul(posW, iViewProj);
	
    return vout;
}

PixelOut PS(PixelIn pin) : SV_Target
{
    float mip = floor(mip_level(pin.TexC, VT_VTSize) - VT_MipBias);
    mip = clamp(mip, 0, log2(VT_PageTableSize));

    const float2 offset = floor(pin.TexC * VT_PageTableSize);
    return float4(float3(offset / exp2(mip), mip), 1.0);
}

#endif
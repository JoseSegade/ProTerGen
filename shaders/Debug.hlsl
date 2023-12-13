#ifndef _LINE_HLSL_
#define _LINE_HLSL_

#include "Common.hlsli"

struct VertexIn
{
    float4 Pos : POSITION;
};

struct VertexOut
{
    float4 Pos : SV_POSITION;
};

typedef VertexOut PixelIn;

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0;

    float4 posW = mul(float4(vin.Pos.xyz, 1.0f), Obj_World);

    vout.Pos = mul(posW, iViewProj);
    vout.Pos.z = log(iNearZ * 2.0f * vout.Pos.z + 1.0f) / log(iNearZ * 2.0f * iFarZ + 1.0f) * vout.Pos.w;

    return vout;
}

float4 PS(PixelIn pin) : SV_TARGET
{
	return float4(1.0f, 0.0f, 1.0f, 1.0f);
}

#endif
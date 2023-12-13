#ifndef _INSTANCE_BASIC_H_
#define _INSTANCE_BASIC_H_

#include "Common.hlsli"

struct VertexIn
{
	float4 Position : POSITION;
	float3 Normal : NORMAL;
	float2 TexCoord : TEXCOORD;
	float3 Tangent : TANGENT;
    float3 InstancePosition : INSTANCE_POSITION;
};

struct VertexOut
{
    float4 Position : SV_POSITION;
};

typedef VertexOut PixelIn;

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0;
	
    float4 pos = mul(float4(vin.Position.xyz, 1.0f), Obj_World);
    pos.xyz += vin.InstancePosition;
    vout.Position = mul(pos, iViewProj);
	
    vout.Position.z = 2.0 * log(vout.Position.w / iNearZ) / log(iFarZ / iNearZ) - 1;
    vout.Position.z *= vout.Position.w;

    return vout;
}

float4 PS() : SV_TARGET
{
	return float4(1.0f, 0.0f, 1.0f, 1.0f);
}

#endif
#ifndef _SHADOW_CASTER_HLSL_
#define _SHADOW_CASTER_HLSL_

#include "Common.hlsli"
#include "VirtualTexture.hlsli"

struct VertexIn
{
    float4 Position : POSITION;
    float2 TexCoords : TEXCOORD0;
};

struct VertexOut
{
    float4 Position : SV_POSITION;
};

typedef VertexOut PixelIn;

#ifdef TERRAIN_SHADOW_CASTER

struct HullOut
{
	float4 PosL     : POSITION0;
    float3 NormalL  : NORMAL0;
	float2 TexC     : TEXCOORD0;
	float3 TangentU : TANGENT0;
};

typedef PixelIn DomainOut;

struct PatchConstantOut
{
    float Edges[3] : SV_TessFactor;
    float Inside   : SV_InsideTessFactor;
};

struct DomainIn
{
    PatchConstantOut DomainIn;    
    float3 UWVCoord : SV_DomainLocation;
};

[domain("tri")]
DomainOut DS(OutputPatch<HullOut, 3> patch, DomainIn din)
{
    DomainOut dout = (DomainOut) 0;
    
    float4 vertexPos = din.UWVCoord.x * patch[0].PosL + din.UWVCoord.y * patch[1].PosL + din.UWVCoord.z * patch[2].PosL;
    float2 texC      = din.UWVCoord.x * patch[0].TexC + din.UWVCoord.y * patch[1].TexC + din.UWVCoord.z * patch[2].TexC;
    
    virtualtexture_scale scale; 
    scale.atlas_scale   = VT_AtlasScale;
    scale.border_scale  = VT_BorderScale;
    scale.border_offset = VT_BorderOffset;
    float mip0, mip1    = 0;
    const float inter   = get_inter(vertexPos.w, mip0, mip1);
    const float2 uv0    = get_sample(texC, mip0, tIndirectionTexture, VT_PageTableSize, scale, Sampler_PointClamp);
    const float2 uv1    = get_sample(texC, mip1, tIndirectionTexture, VT_PageTableSize, scale, Sampler_PointClamp);
    const float height0 = tAtlasTexture[0].SampleLevel(Sampler_AnisotropicWrap, uv0, 0).a; 
    const float height1 = tAtlasTexture[0].SampleLevel(Sampler_AnisotropicWrap, uv1, 0).a; 
    const float height = lerp(height0, height1, inter);
    vertexPos.y = height * Ter_Height; 
    
    float3 outW   = vertexPos.xyz;
    outW          = mul(float4(outW, 1.0f), Obj_World).xyz;
    dout.Position = mul(float4(outW, 1.0f), PerShadow_Mat);
    
    return dout;
}

#else

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0;

    float3 outW = vin.Position.xyz;
    outW = mul(float4(outW, 1.f), Obj_World).xyz;
    vout.Position    = mul(float4(outW, 1.0f), PerShadow_Mat);

	return vout;
}
#endif

float PS(PixelIn pin) : SV_DEPTH
{
    return pin.Position.z;
}

#endif
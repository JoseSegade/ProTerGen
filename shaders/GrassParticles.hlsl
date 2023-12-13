#ifndef _GRASS_PARTICLES_HLSL_
#define _GRASS_PARTICLES_HLSL_

#include "Common.hlsli"
#include "VirtualTexture.hlsli"

struct VSInput
{
    float3 pos : POSITION;
};

struct VSOuput
{
    float4 pos : SV_POSITION;
};

typedef VSOuput GSInput;

struct GSOutput
{
	float4 pos : SV_POSITION;
    float2 uvs : TEXCOORD;
};

typedef GSOutput PSInput;

VSOuput VS(VSInput vin)
{
    VSOuput vout = (VSOuput) 0;
	
    vin.pos.xz *= Ter_TerrainSize;
    vout.pos = mul(float4(vin.pos, 1.0f), Obj_World);

    virtualtexture_scale scale;
    scale.atlas_scale   = VT_AtlasScale;
    scale.border_scale  = VT_BorderScale;
    scale.border_offset = VT_BorderOffset;
    const float2 vt     = (vin.pos.xz + Ter_TerrainSize * 0.5f) / Ter_TerrainSize;
    const float mip     = 0;
    const float2 uv     = get_sample(vt, mip, tIndirectionTexture, VT_PageTableSize, scale, Sampler_PointClamp);
    const float height  = Ter_Height * tAtlasTexture[0].SampleLevel(Sampler_PointWrap, uv, 0).a;
    vout.pos.y = height;

    return vout;
}

[maxvertexcount(4)]
void GS(point GSInput gin[1], inout TriangleStream<GSOutput> gout)
{
    float3 toCamera = normalize(gin[0].pos.xyz - iEyePosW);
    toCamera.y = 0.0f;
    float3 right = -normalize(cross(float3(0.0f, 1.0f, 0.0f), toCamera));

    float size = 20.0f;
    float4 vertices[4] =
    {
        float4(gin[0].pos.xyz - size * right, 1.0f),
        float4(gin[0].pos.xyz + size * right, 1.0f),
        float4(gin[0].pos.xyz - size * right + float3(0.0f, size, 0.0f), 1.0f),
        float4(gin[0].pos.xyz + size * right + float3(0.0f, size, 0.0f), 1.0f),
    };
    
    float2 uvs[4] =
    {
        float2(1.0f, 1.0f),
        float2(0.0f, 1.0f),
        float2(1.0f, 0.0f),
        float2(0.0f, 0.0f),
    };

    [unroll]
	for (uint i = 0; i < 4; i++)
	{
		GSOutput element;
        element.pos = mul(vertices[i], iViewProj);
        element.pos.z = log(iNearZ * 2.0f * element.pos.z + 1.0f) / log(iNearZ * 2.0f * iFarZ + 1.0f) * element.pos.w;

        element.uvs = uvs[i];
		gout.Append(element);
	}
}

float4 PS(PSInput pin) : SV_TARGET
{
    MaterialData mat = Mat_Data[Obj_MaterialIndex];
    uint dMatIdx = mat.DiffuseMapIndex;
    float4 c = tTextureMaps[dMatIdx].Sample(Sampler_AnisotropicWrap, pin.uvs);
    if(c.a < 0.1) discard;
    return c;
}

#endif
#ifndef _COMMON_HLSLI_
#define _COMMON_HLSLI_

#ifndef GAMMA
#define GAMMA 0.7
#endif

#ifdef COLOR_PALETTE
// black, white, red, green, blue, cyan, magenta, yellow, orange, pink
static float4 color_palette[10] =
{
    { 0.0f, 0.0f, 0.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 1.0f, 0.0f, 1.0f },
    { 0.0f, 0.0f, 1.0f, 1.0f },
    { 0.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 0.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 0.0f, 1.0f },
    { 1.0f, 0.5f, 0.0f, 1.0f },
    { 1.0f, 0.0f, 0.5f, 1.0f },
};
#endif

struct Light
{
    float3 Strength;
    float _LightPad0;
    float3 Direction;
    float _LightPad1;    
};

struct Environment
{
    float4 AmbientLight;
    float4 SkyColor;
    float4 VanishingColor;
    float4 HorizonColor;
    float4 CloudColor;
    float4 FogColor;
    float  FogOpacity;
    float2 CloudOffset;
    float  CloudScale;
};

struct MaterialData
{
	float4   DiffuseAlbedo;
	float4x4 MatTransform;
	float    Roughness;
	float    Fresnel;
	uint     DiffuseMapIndex;
	uint     NormalMapIndex;
};

struct TerrainMaterialLayer
{
    int   MaterialIndex;
    float MinSlope;
    float SlopeSmoothness;
    float MinHeight;
    float HeightSmoothness;
};

struct ShadowSplit
{
    float4x4 ViewProjMat;
    float    SplitDistance;
};

#ifndef NUM_SHADOW_SPLIT
#define NUM_SHADOW_SPLIT 1
#endif
#ifndef NUM_ATLAS_TEXTURES
#define NUM_ATLAS_TEXTURES 1
#endif
#ifndef NUM_TERRAIN_MATERIAL_LAYERS
#define NUM_TERRAIN_MATERIAL_LAYERS 8
#endif
#ifndef INVALID_INDEX
#define INVALID_INDEX 0xffffffff
#endif
#ifndef NUM_TEXTURE_MAPS
#define NUM_TEXTURE_MAPS 18
#define NEXT_TEXTURE_MAPS_REGISTER t18
#endif

Texture2D tTextureMaps[NUM_TEXTURE_MAPS] : register(t0);
Texture2D tShadowMaps [NUM_SHADOW_SPLIT] : register(NEXT_TEXTURE_MAPS_REGISTER);

Texture2D tIndirectionTexture               : register(t0, space1);
Texture2D tAtlasTexture[NUM_ATLAS_TEXTURES] : register(t1, space1);

StructuredBuffer<MaterialData>         Mat_Data    : register(t0, space2);
StructuredBuffer<ShadowSplit>          Shadow_Data : register(t1, space2);
StructuredBuffer<TerrainMaterialLayer> LayerData   : register(t2, space2);

SamplerState Sampler_PointWrap        : register(s0);
SamplerState Sampler_PointClamp       : register(s1);
SamplerState Sampler_LinearWrap       : register(s2);
SamplerState Sampler_LinearClamp      : register(s3);
SamplerState Sampler_AnisotropicWrap  : register(s4);
SamplerState Sampler_AnisotropicClamp : register(s5);
SamplerComparisonState Sampler_Shadow : register(s6);

cbuffer cbPerObject : register(b0)
{
    float4x4 Obj_World;
	float4x4 Obj_TexTransform;
    uint     Obj_MaterialIndex;
    uint     Obj_NumTerrainMaterialLayers;
    uint2    _ObjPad0;
};

cbuffer cbPass : register(b1)
{
    float4x4 iView;
    float4x4 iInvView;
    float4x4 iProj;
    float4x4 iInvProj;
    float4x4 iViewProj;
    float4x4 iInvViewProj;
    float4x4 iInvProjView;
    
    float2 iRenderTargetSize;
    float2 iInvRenderTargetSize;
    
    float3 iEyePosW;
    float iNearZ;
    
    float iFarZ;
    float iTotalTime;
    float iDeltaTime;
    float _PassPad0;
    
    Environment iEnvironment;
    Light iSun;    
};

cbuffer cbVirtualTexture : register(b2)
{
    float VT_VTSize;
    float VT_PageTableSize;
    float VT_AtlasScale;
    float VT_BorderScale;    
    float VT_BorderOffset;
    float VT_MipBias;    
    uint  VT_AtlasIndex;
    uint  VT_PageTableIndex;
};

cbuffer cbTerrain : register(b3)
{
    float Ter_TerrainSize;
    float Ter_TerrianChunkSize;
    float Ter_Height;
    float Ter_Subdivision;
}

cbuffer cbShadow : register(b4)
{
    float  Shadow_Bias;
    float3 _ShadowPad0;
}

cbuffer cbPerShadow : register(b5)
{
    float4x4 PerShadow_Mat;
}

#endif
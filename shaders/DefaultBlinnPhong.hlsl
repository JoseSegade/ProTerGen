#ifndef _DEFAULT_BLINN_PHONG_HLSI_
#define _DEFAULT_BLINN_PHONG_HLSL_

#include "LightingUtil.hlsli"

#ifdef ALPHA_CLIP
#ifndef ALPHA_CLIP_THRESHOLD
#define ALPHA_CLIP_THRESHOLD 0.1
#endif
#endif

struct VertexIn
{
	float4 PosL     : POSITION0;
    float3 NormalL  : NORMAL0;
	float2 TexC     : TEXCOORD0;
	float3 TangentU : TANGENT0;
};

struct VertexOut
{
	float4 PosH     : SV_POSITION;
    float4 PosW     : POSITION0;
    float3 NormalW  : NORMAL0;
    float2 TexC     : TEXCOORD0;
    float3 TangentW : TANGENT0;
};

typedef VertexOut PixelIn;

struct PixelOut
{
    float4 Color : SV_TARGET;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0;

    const float3 outW = mul(float4(vin.PosL.xyz, 1.f), Obj_World).xyz;
    vout.PosW = float4(outW.xyz, vin.PosL.w);
    vout.PosH = mul(float4(outW, 1.0f), iViewProj);

    // Logaritmic depth - Sanates z fighting for far distance objects
    vout.PosH.z = log(iNearZ * 2.0f * vout.PosH.z + 1.0f) / log(iNearZ * 2.0f * iFarZ + 1.0f) * vout.PosH.w;
    
    vout.NormalW  = mul(vin.NormalL, (float3x3) Obj_World); 
    vout.TangentW = mul(vin.TangentU, (float3x3) Obj_World);
    
    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), Obj_TexTransform).xy;
    
    return vout;
}

PixelOut PS(PixelIn pin)
{
    PixelOut pout = (PixelOut) 0;

    const MaterialData matData = Mat_Data[Obj_MaterialIndex];

    const float4 ctex = tTextureMaps[matData.DiffuseMapIndex].Sample(Sampler_AnisotropicWrap, pin.TexC);
#ifdef ALPHA_CLIP
    if(ctex.a < ALPHA_CLIP_THRESHOLD)
    {
        discard;
    }
#endif
    const float4 ntex = tTextureMaps[matData.NormalMapIndex].Sample(Sampler_AnisotropicWrap, pin.TexC);
    const float3 n    = normalize(pin.NormalW);
    const float3 t    = normalize(pin.TangentW);

    const float3 toEyeW  = normalize(iEyePosW - pin.PosW.xyz);    
    const float4 ambient = iEnvironment.AmbientLight * matData.DiffuseAlbedo * ctex;

	const float fresnel   = matData.Fresnel;
    const float shininess = (1.0f - matData.Roughness);
    const Material mat    = { ctex * matData.DiffuseAlbedo, shininess, fresnel };
    
    const float3 bumpedNormalW = normal_sample_to_world_space(ntex.rgb, n, t);
    const float3 directLight   = directional_light(pin.PosW.xyz,
        bumpedNormalW, iSun, mat);

    pout.Color = ambient + float4(directLight, 0.0f);
    
    const float3 r              = reflect(-toEyeW, bumpedNormalW);
    const float3 fresnelFactor  = schlick_fresnel(bumpedNormalW, r, fresnel);
    pout.Color.rgb             += shininess * fresnelFactor * iEnvironment.SkyColor.rgb;

    pout.Color.rgb = fog(pout.Color.rgb, iEnvironment.FogColor, pin.PosW.xyz, iEyePosW, iFarZ);
	
    pout.Color.a = ctex.a;

    return pout;
}

#endif
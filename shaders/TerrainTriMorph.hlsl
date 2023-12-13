#ifndef _TERRAIN_TRI_MORPH_HLSL_
#define _TERRAIN_TRI_MORPH_HLSL_

#ifdef DEBUG_LOD_LEVEL
#define COLOR_PALETTE
#endif

#include "LightingUtil.hlsli"
#include "Shadow.hlsli"
#include "VirtualTexture.hlsli"

struct VertexIn
{
	float4 PosL     : POSITION0;
    float3 NormalL  : NORMAL0;
	float2 TexC     : TEXCOORD0;
	float3 TangentU : TANGENT0;
};

struct VertexOut
{
    float4 PosL : SV_POSITION;
    float2 TexC : TEXCOORD0;
};

struct PatchConstantIn
{    
    uint PatchID : SV_PrimitiveID;
};

struct PatchConstantOut
{
    float Edges[3] : SV_TessFactor;
    float Inside   : SV_InsideTessFactor;
};

struct HullIn
{   
    uint PointId : SV_OutputControlPointID;
    uint PatchId : SV_PrimitiveID;
};

typedef VertexIn HullOut;

struct DomainIn
{
    PatchConstantOut DomainIn;    
    float3 UWVCoord : SV_DomainLocation;
};

struct PixelIn
{
	float4 PosH      : SV_POSITION;
    float4 PosW      : POSITION0;
    float4 ShadowPos : POSITION1;
	float2 TexC      : TEXCOORD0;
};

typedef PixelIn DomainOut;
typedef float4 PixelOut;

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f;
    
    vout.PosL     = vin.PosL;
    vout.TexC     = vin.TexC;

    return vout;
}

PatchConstantOut PatchConstantFunction(InputPatch<VertexOut, 3> patch, PatchConstantIn pcin)
{
    PatchConstantOut pcout = (PatchConstantOut) 0;
    
    pcout.Edges[0] = Ter_Subdivision;
    pcout.Edges[1] = Ter_Subdivision;
    pcout.Edges[2] = Ter_Subdivision;
    
    pcout.Inside = Ter_Subdivision;
    
    return pcout;
}

[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("PatchConstantFunction")]
HullOut HS( InputPatch<VertexOut, 3> patch, HullIn hin)
{
    HullOut hout = (HullOut) 0;
    
    hout.PosL     = patch[hin.PointId].PosL;
    hout.TexC     = patch[hin.PointId].TexC;
    
    return hout;
}

[domain("tri")]
DomainOut DS(OutputPatch<HullOut, 3> patch, DomainIn din)
{
    DomainOut dout = (DomainOut) 0;
    
    float4 vertexPos = din.UWVCoord.x * patch[0].PosL     + din.UWVCoord.y * patch[1].PosL     + din.UWVCoord.z * patch[2].PosL;
    float2 texC      = din.UWVCoord.x * patch[0].TexC     + din.UWVCoord.y * patch[1].TexC     + din.UWVCoord.z * patch[2].TexC;
    
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
    const float height  = lerp(height0, height1, inter);
    vertexPos.y = height * Ter_Height; 
    
    float3 outW = vertexPos.xyz;
    outW        = mul(float4(outW, 1.f), Obj_World).xyz;
    dout.PosW   = float4(outW.xyz, vertexPos.w);
    dout.PosH   = mul(float4(outW, 1.0f), iViewProj);

    dout.ShadowPos = mul(float4(dout.PosW.xyz, 1.0f), Shadow_Data[0].ViewProjMat);
    
    // Logaritmic depth - Sanates z fighting for far distance objects
    dout.PosH.z = log(iNearZ * 2.0f * dout.PosH.z + 1.0f) / log(iNearZ * 2.0f * iFarZ + 1.0f) * dout.PosH.w;
    
    dout.TexC = mul(float4(texC, 0.0f, 1.0f), Obj_TexTransform).xy;
    
    return dout;
}

float4 hash4(float2 p)
{
    return
    frac(sin(float4(1.0f + dot(p, float2(37.0f, 17.0f)),
                    2.0f + dot(p, float2(11.0f, 47.0f)),
                    3.0f + dot(p, float2(41.0f, 29.0f)),
                    4.0f + dot(p, float2(23.0f, 31.0f)))) * 103.0f);
}

float4 texture_no_tile(Texture2D tex, float2 uv)
{
    const float2 flooruv = floor(uv);
    const float2 fractuv = frac(uv);

    float4 of0 = hash4(flooruv + float2(0.0f, 0.0f));
    float4 of1 = hash4(flooruv + float2(1.0f, 0.0f));
    float4 of2 = hash4(flooruv + float2(0.0f, 1.0f));
    float4 of3 = hash4(flooruv + float2(1.0f, 1.0f));

    const float2 dx = ddx(uv);
    const float2 dy = ddy(uv);

    of0.zw = sign(of0.zw - 0.5f);
    of1.zw = sign(of1.zw - 0.5f);
    of2.zw = sign(of2.zw - 0.5f);
    of3.zw = sign(of3.zw - 0.5f);

    const float2 uv0 = uv * of0.zw + of0.xy;
    const float2 uv1 = uv * of1.zw + of1.xy;
    const float2 uv2 = uv * of2.zw + of2.xy;
    const float2 uv3 = uv * of3.zw + of3.xy;

    const float2 dx0 = dx * of0.zw;
    const float2 dx1 = dx * of1.zw;
    const float2 dx2 = dx * of2.zw;
    const float2 dx3 = dx * of3.zw;

    const float2 dy0 = dy * of0.zw;
    const float2 dy1 = dy * of1.zw;
    const float2 dy2 = dy * of2.zw;
    const float2 dy3 = dy * of3.zw;

    const float2 b = smoothstep(0.25f, 0.75f, fractuv);

    const float2 o = float2(0.0f, 0.0f);
    return lerp(lerp(tex.SampleGrad(Sampler_AnisotropicWrap, uv0, dx0, dy0, o),
                     tex.SampleGrad(Sampler_AnisotropicWrap, uv1, dx1, dy1, o), b.x),
                lerp(tex.SampleGrad(Sampler_AnisotropicWrap, uv2, dx2, dy2, o),
                     tex.SampleGrad(Sampler_AnisotropicWrap, uv3, dx3, dy3, o), b.x), b.y);
}

float2 blend_factor(float4 tex0, float4 tex1, float mask)
{
    float t0 = (tex0.x + tex0.y + tex0.z) / 3.0f;
    float t1 = (tex1.x + tex1.y + tex1.z) / 3.0f;
    float a0 = (1.0f - mask);
    float a1 = mask;

    float depth = 0.1f;

    float ma = max(t0 + a0, t1 + a1) - depth;
    float b0 = max(t0 + a0 - ma, 0);
    float b1 = max(t1 + a1 - ma, 0);

    return float2(b0, b1);
}

float4 blend_by_blend_factor(float4 val0, float4 val1, float2 blend_factor)
{
    return (val0 * blend_factor.x + val1 * blend_factor.y) / (blend_factor.x + blend_factor.y);
}

float4 get_vt_value(float mip, float2 tex_coords)
{
    virtualtexture_scale scale; 
    scale.atlas_scale   = VT_AtlasScale;
    scale.border_scale  = VT_BorderScale;
    scale.border_offset = VT_BorderOffset;
    float        mip0   = 0.0f;
    float        mip1   = 0.0f;
    const float  inter  = get_inter(mip, mip0, mip1);
    const float2 uv0    = get_sample(tex_coords, mip0, tIndirectionTexture, VT_PageTableSize, scale, Sampler_PointClamp);
    const float2 uv1    = get_sample(tex_coords, mip1, tIndirectionTexture, VT_PageTableSize, scale, Sampler_PointClamp);
    const float4 val1   = tAtlasTexture[0].SampleLevel(Sampler_AnisotropicWrap, uv1, 0); 
    const float4 val0   = tAtlasTexture[0].SampleLevel(Sampler_AnisotropicWrap, uv0, 0) ; 
    return lerp(val0, val1, inter);
}

#ifdef DEBUG_LOD_LEVEL 

PixelOut PS(PixelIn pin) : SV_Target
{
    float4 outColor = float4(0.0f, 0.0f, 0.0f, 1.0f);

    const float4 slopeDerivativesHeight = get_vt_value(pin.PosW.w, pin.TexC);
    const float  terSlope               = slopeDerivativesHeight.x;
    const float2 terDerivatives         = slopeDerivativesHeight.yz;
    const float  terHeight              = slopeDerivativesHeight.w;

	// Fetch the material data.
	MaterialData matData = Mat_Data[Obj_MaterialIndex];
	float4 diffuseAlbedo = matData.DiffuseAlbedo;
	float fresnel        = matData.Fresnel;
	float roughness      = matData.Roughness;
	uint diffuseMapIndex = matData.DiffuseMapIndex;
	uint normalMapIndex  = matData.NormalMapIndex;
    
    float mip0  = 0;
    float mip1  = 0;
    const float inter = get_inter(pin.PosW.w, mip0, mip1);
    float4 color0 = color_palette[(mip0 + 3) % 10];
    float4 color1 = color_palette[(mip1 + 3) % 10];
    float4 color  = lerp(color0, color1, inter);
    diffuseAlbedo = color;


    float2 der2 = Ter_Height * sign(terDerivatives) * terDerivatives * terDerivatives;
    float3 terNormal = normalize(float3(der2.x , 1.0f, -der2.y));
	
    float4 normalMapSample = float4(terNormal, 0.05f);
    float3 bumpedNormalW   = normalMapSample.xyz;

    float3 toEyeW = normalize(iEyePosW - pin.PosW.xyz);    

    float4 ambient = iEnvironment.AmbientLight * diffuseAlbedo;    

    const float shininess = (1.0f - roughness) * normalMapSample.a;
    Material mat = { diffuseAlbedo, shininess, fresnel };
    
    float3 directLight = directional_light(pin.PosW.xyz,
        bumpedNormalW, iSun, mat);

    outColor = ambient + float4(directLight, 0.0f);
    
    float3 r = reflect(-toEyeW, bumpedNormalW);
    //float4 reflectionColor = tCubeMap.Sample(Sampler_LinearWrap, r);
    float3 fresnelFactor = schlick_fresnel(bumpedNormalW, r, fresnel);
    outColor.rgb += shininess * fresnelFactor * iEnvironment.SkyColor.rgb;

    outColor.rgb = fog(outColor.rgb, iEnvironment.FogColor, pin.PosW.xyz, iEyePosW, iFarZ);
	
    //litColor.rgb = lerp(litColor.rgb, float3(1.0f, 0.0f, 1.0f), ceil(shadow(pin.ShadowPos, 0) * 2.0f));    

    outColor.a = diffuseAlbedo.a;
    
    return outColor;
}

#else

PixelOut PS(PixelIn pin) : SV_Target
{
    float4 outColor = float4(0.0f, 0.0f, 0.0f, 1.0f);

    const float4 slopeDerivativesHeight = get_vt_value(pin.PosW.w, pin.TexC);
    const float  terSlope               = slopeDerivativesHeight.x;
    const float2 terDerivatives         = slopeDerivativesHeight.yz;
    const float  terHeight              = slopeDerivativesHeight.w;

    float2 der2      = Ter_Height * sign(terDerivatives) * terDerivatives * terDerivatives;
    float3 terNormal = normalize(float3(der2.x , 1.0f, -der2.y));
    
    float mask0 = clamp(Ter_Height * pow(terSlope, 1.2f), 0.001f, 1.0f);

    TerrainMaterialLayer tml                  = LayerData[0];
    MaterialData         lmat                 = Mat_Data[tml.MaterialIndex];
    float4               blendedDiffuseAlbedo = lmat.DiffuseAlbedo;
    float                blendedRoughness     = lmat.Roughness;
    float                blendedFresnel       = lmat.Fresnel;
    float4               blendedCTex          = texture_no_tile(tTextureMaps[lmat.DiffuseMapIndex], 2500.0f * pin.TexC);
    float4               blendedNTex          = texture_no_tile(tTextureMaps[lmat.NormalMapIndex],  2500.0f * pin.TexC);

    const uint layerCount    = min(Obj_NumTerrainMaterialLayers, NUM_TERRAIN_MATERIAL_LAYERS);
    for (uint i = 1; i < layerCount; ++i)
    {
        tml  = LayerData[i];
        lmat = Mat_Data[tml.MaterialIndex];

        const int    idxColor    = max(0, lmat.DiffuseMapIndex);
        const int    idxNormal   = max(0, lmat.NormalMapIndex);
        const float4 tex         = texture_no_tile(tTextureMaps[idxColor],  2500.0f * pin.TexC);
        const float4 nor         = texture_no_tile(tTextureMaps[idxNormal], 2500.0f * pin.TexC);
        const float  heightScale = 1.0f / max(0.001f, (1.0f - tml.MinHeight) * tml.HeightSmoothness);
        const float  heightMask  = min(1.0f, max(0.0f, (terHeight - tml.MinHeight) * heightScale));
        const float  slopeScale  = 1.0f / max(0.001f, (tml.SlopeSmoothness) * tml.MinSlope);
        const float  slopeMask   = min(1.0f, max(0.0f, (mask0 - (1.0f - tml.SlopeSmoothness)) * slopeScale));
        const float2 blendFactor = blend_factor(blendedCTex, tex, heightMask * slopeMask);

        blendedDiffuseAlbedo = blend_by_blend_factor(blendedDiffuseAlbedo,  lmat.DiffuseAlbedo, blendFactor);
        blendedRoughness     = blend_by_blend_factor(    blendedRoughness, lmat.Roughness.xxxx, blendFactor).x;
        blendedFresnel       = blend_by_blend_factor(      blendedFresnel,   lmat.Fresnel.xxxx, blendFactor).x;
        blendedCTex          = blend_by_blend_factor(         blendedCTex,                 tex, blendFactor);
        blendedNTex          = blend_by_blend_factor(         blendedNTex,                 nor, blendFactor);
    }
	
    float3 nor = normalize(blendedNTex.rgb * 2.0f - 1.0f).rbg;
    nor = terNormal;

    const float3   toEyeW      = normalize(iEyePosW - pin.PosW.xyz);    
    const float4   colorAlbedo = blendedDiffuseAlbedo * blendedCTex;
    const float    spec        = (1.0f - blendedRoughness) * 0.05f;
    const Material mat         = { colorAlbedo, spec, blendedFresnel };
    
    const float4 ambient        = iEnvironment.AmbientLight * colorAlbedo;    
    const float3 directLight    = directional_light(pin.PosW.xyz, nor, iSun, mat);
    const float3 r              = reflect(-toEyeW, nor);
    const float3 fresnelFactor  = schlick_fresnel(nor, r, blendedFresnel);
    const float3 specReflection = spec * fresnelFactor * iEnvironment.SkyColor.rgb;

    outColor.rgb = ambient.rgb + directLight + specReflection;
    outColor.rgb = fog(outColor.rgb, iEnvironment.FogColor, pin.PosW.xyz, iEyePosW, iFarZ);
    outColor.a   = blendedDiffuseAlbedo.a;
    
    return outColor;
}

#endif

#endif

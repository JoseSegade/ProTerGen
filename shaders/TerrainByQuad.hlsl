#ifndef _TERRAIN_BY_QUAD_HLSL_
#define	_TERRAIN_BY_QUAD_HLSL_

#define COLOR_PALETTE

#include "Common.hlsli"
#include "VirtualTexture.hlsli"

#define SUBDIVISIONS_I 256 
#define SUBDIVISIONS_O 256 

struct VertexIn
{
    float4 Pos       : POSITION;
    float2 TexCoords : TEXCOORD0;
};

typedef VertexIn VertexOut;

struct PatchConstantIn
{    
    uint PatchID : SV_PrimitiveID;
};

struct PatchConstantOut
{
    float  Edges[4]  : SV_TessFactor;
    float  Inside[2] : SV_InsideTessFactor;
};

struct HullIn
{   
    uint PointId : SV_OutputControlPointID;
    uint PatchId : SV_PrimitiveID;
};

typedef VertexOut HullOut;

struct DomainIn
{
    PatchConstantOut domainIn;    
    float2           uv : SV_DomainLocation;
};

struct PixelIn
{
	float4 PosH      : SV_POSITION;
    float4 PosW      : POSITION0;
	float2 TexC      : TEXCOORD0;
};

typedef PixelIn DomainOut;

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0;
    
    vout.Pos       = vin.Pos;
    vout.TexCoords = vin.TexCoords;

    return vout;
}

PatchConstantOut PatchConstantFunction(InputPatch<VertexOut, 1> patch, PatchConstantIn pcin)
{
    PatchConstantOut pcout = (PatchConstantOut) 0;
    
    pcout.Edges[0] = SUBDIVISIONS_O;
    pcout.Edges[1] = SUBDIVISIONS_O;
    pcout.Edges[2] = SUBDIVISIONS_O;
    pcout.Edges[3] = SUBDIVISIONS_O;
    
    pcout.Inside[0] = SUBDIVISIONS_I;
    pcout.Inside[1] = SUBDIVISIONS_I;

    return pcout;
}

float map(float value, float oldMin, float oldMax, float newMin, float newMax)
{
    const float oldDiff = oldMax - oldMin;
    const float newDiff = newMax - newMin;
    return (((value - oldMin) / oldDiff) * newDiff) + newMin;
}

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_ccw")]
[outputcontrolpoints(4)]
[patchconstantfunc("PatchConstantFunction")]
HullOut HS(InputPatch<VertexOut, 1> patch, HullIn hin)
{
    HullOut hout = (HullOut) 0;
    const float ps = 8.0f;
    const float us = 0.0625f;

    float4 pd = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float2 ud = float2(0.0f, 0.0f);
    if (hin.PointId == 0)
    {
        pd = float4(-ps, 0.0f, -ps, 0.0f);
        ud = float2(0.0f, 0.0f);
    }
    else if(hin.PointId == 1)
    {
        pd = float4(ps, 0.0f, -ps, 0.0f);
        ud = float2(us, 0.0f);
    }
    else if(hin.PointId == 2)
    {
        pd = float4(-ps, 0.0f, ps, 0.0f);
        ud = float2(0.0f, us);
    }
    else if(hin.PointId == 3)
    {
        pd = float4(ps, 0.0f, ps, 0.0f);
        ud = float2(us, us);
    }
    hout.Pos       = patch[hin.PatchId].Pos + pd;
    hout.TexCoords = patch[hin.PatchId].TexCoords + ud;
    
    return hout;
}

[domain("quad")]
DomainOut DS(OutputPatch<HullOut, 4> patch, DomainIn din)
{
    DomainOut dout = (DomainOut) 0;
    
    /*
    const float dis = patch[0].Pos.w       * (1.0f - din.uv.x) * (1.0f - din.uv.y) +
                      patch[1].Pos.w       * (       din.uv.x) * (1.0f - din.uv.y) +
                      patch[2].Pos.w       * (1.0f - din.uv.x) * (       din.uv.y) +
                      patch[3].Pos.w       * (       din.uv.x) * (       din.uv.y);
    const float fldis = floor(dis);
    const float frdis = frac(dis);
    
    const float d = 8.0f / float(SUBDIVISIONS_O);
    const float move = map(frdis, 0.0f, 1.0f, 0.0f, d);
    */

    float4 pos = patch[0].Pos * (1.0f - din.uv.x) * (1.0f - din.uv.y) +
                 patch[1].Pos * (       din.uv.x) * (1.0f - din.uv.y) +
                 patch[2].Pos * (1.0f - din.uv.x) * (       din.uv.y) +
                 patch[3].Pos * (       din.uv.x) * (       din.uv.y);

    float2 uvs = patch[0].TexCoords * (1.0f - din.uv.x) * (1.0f - din.uv.y) +
                 patch[1].TexCoords * (       din.uv.x) * (1.0f - din.uv.y) +
                 patch[2].TexCoords * (1.0f - din.uv.x) * (       din.uv.y) +
                 patch[3].TexCoords * (       din.uv.x) * (       din.uv.y);
    
    float3 outW = pos.xyz;
    outW        = mul(float4(outW, 1.f), Obj_World).xyz;
    dout.PosW   = float4(outW.xyz, pos.w);
    dout.PosH   = mul(float4(outW, 1.0f), iViewProj);
    
    dout.PosH.z = log(iNearZ * 2.0f * dout.PosH.z + 1.0f) / log(iNearZ * 2.0f * iFarZ + 1.0f) * dout.PosH.w;
    
    dout.TexC = mul(float4(uvs, 0.0f, 1.0f), Obj_TexTransform).xy;
                
    return dout;
}

float4 PS(PixelIn pin) : SV_TARGET
{
    return color_palette[((pin.PosW.w / 25.0f) + 2) % 10];
}

#endif
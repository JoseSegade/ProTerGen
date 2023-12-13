#ifndef _SKY_HLSL_
#define _SKY_HLSL_

#include "Common.hlsli"
#include "Noise.hlsli"

struct VertexIn
{
	float3 Pos : POSITION;
};

struct VertexOut
{
	float4 Pos : SV_POSITION;
    float4 PosW : POSITION0;
};

typedef VertexOut PixelIn;

struct PixelOut
{
    float4 Color : SV_Target;
    float Depth : SV_Depth;
};

VertexOut VS(VertexIn vin)
{
    VertexOut output = (VertexOut) 0;
    
    float4x4 viewMinusTrans = iView;
    viewMinusTrans[3].xyz = float3(0.0f, 0.0f, 0.0f);

    output.PosW = mul(float4(vin.Pos, 1.0f), Obj_World);
    float4 p = float4(output.PosW.xyz + iEyePosW, 1.0f);
    output.Pos = mul(p, iViewProj).xyww;
    
    return output;
}

PixelOut PS(PixelIn pin)
{ 
    PixelOut pout = (PixelOut) 0;
    
    float3 rayDir = normalize(pin.PosW.xyz);
    rayDir.z *= -1;
    float sundot = saturate(dot(rayDir, -iSun.Direction));

    float3 color = iEnvironment.SkyColor.rgb;
    color = lerp(color, iEnvironment.VanishingColor.a * iEnvironment.VanishingColor.rgb, pow(1.0f - max(rayDir.y, 0.0f), 4.0f));
    color = lerp(color, iEnvironment.HorizonColor.a   * iEnvironment.HorizonColor.rgb,   pow(1.0f - max(rayDir.y, 0.0f), 16.0f));
    
    color += 0.40f * iSun.Strength * pow(sundot,  128.0f);
    color += 0.40f * iSun.Strength * pow(sundot, 512.0f);
    color += 0.35f * iSun.Strength * pow(sundot, 1024.0f);
    
    float2 sc = iEyePosW.xz + rayDir.xz * (5000.0f - iEyePosW.y) / rayDir.y;
    const float cloud = 1.2f * fbm_4((1.0f - iEnvironment.CloudScale) * sc * 0.0025f + iEnvironment.CloudOffset);
    color = lerp
    (
        color,
        (0.5f + sign(-rayDir.y) * 0.5f) * color + (0.5f + sign(rayDir.y) * 0.5f) * 0.8f * iEnvironment.CloudColor.rgb,
        iEnvironment.CloudColor.a * smoothstep(0.01f, 0.9f, cloud)
    );    

    pout.Color = float4(pow(color , GAMMA), 1.f);
    pout.Depth = 1.0f;
    return pout;
}

#endif

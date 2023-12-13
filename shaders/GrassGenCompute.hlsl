#ifndef _GRASS_GEN_COMPUTE_HLSL_
#define _GRASS_GEN_COMPUTE_HLSL_

#include "CommonCompute.hlsli"

StructuredBuffer<float3> tPool : register(t0, space1);

RWStructuredBuffer<float3> uPositions : register(u0);
RWStructuredBuffer<uint> uSize : register(u1);

cbuffer GrassSettings : register(b0)
{
    float3 Grass_EyePos;
    float Grass_Distance;
    uint Grass_PoolSize;
    float3 __grassSettingsPad;
};

struct ComputeIn
{
    uint3 DTid : SV_DispatchThreadID;
    uint3 Gid  : SV_GroupID;
    uint3 GTid : SV_GroupThreadID;
};

#ifndef GROUP_WIDTH
#define GROUP_WIDTH 1
#endif

[numthreads(TG_SIZE_X, TG_SIZE_Y, TG_SIZE_Z)]
void CS( ComputeIn cin )
{
    if (cin.Gid.x - GROUP_WIDTH > 0 || cin.Gid.y - GROUP_WIDTH > 0)
        return;

    const uint p = cin.Gid.x + GROUP_WIDTH * cin.Gid.y;

    const float3 grassPos = tPool[p % Grass_PoolSize];
    const bool cond = distance(grassPos.xz, Grass_EyePos.xz) < Grass_Distance;

    if (cond)
    {
        InterlockedAdd(uSize[0], 1);
        uint i = uSize[0];
        uPositions[p] = float3(1.0f, 2.0f, 3.0f);
    }
}

#endif
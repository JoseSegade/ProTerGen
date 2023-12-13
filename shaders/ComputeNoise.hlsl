
#include "CommonCompute.hlsli"
#include "Noise.hlsli"

SamplerState Sampler_PointClamp : register(s0);

cbuffer Rect : register(b0)
{   
    float RectX;
    float RectY;
    float RectWidth;
    float RectHeight;
    float RectTotalWidth;
    float RectTotalHeight;
    float TileWidth;
    float TileHeight;
};

RWTexture2D<float> tHeightmap : register(u0);

[numthreads(TG_SIZE_X, TG_SIZE_Y, TG_SIZE_Z)]
void CS( uint3 DTid : SV_DispatchThreadID )
{    
    
    const uint tileSize = TileWidth * TileHeight;
    const uint tileCountW = RectWidth / TileWidth;
    const uint2 tileCoords = DTid.xy / uint2(TileWidth, TileHeight);
    const uint tileId = tileCoords.y * tileCountW + tileCoords.x;
    
    const uint2 inTileOffset = DTid.xy % uint2(TileWidth, TileHeight);
    const uint inTileOffsetId = inTileOffset.y * TileWidth + inTileOffset.x;
    
    const uint outTileOffset = tileId * tileSize;
    const uint tileOffset = outTileOffset + inTileOffsetId;
    
    const uint2 pixelCoords = uint2(tileOffset % RectWidth, tileOffset / RectWidth);
    
    const float2 offset = float2(RectX, RectY);
    const float2 size = float2(RectTotalWidth, RectTotalHeight);
    const float2 coords = (DTid.xy + offset) / size;
    const float value = fbm_terrain(coords);
    
    tHeightmap[pixelCoords] = value * value;
    //tHeightmap[DTid.xy] = value * value;
}
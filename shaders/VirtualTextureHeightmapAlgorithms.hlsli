#ifndef _VIRTUAL_TEXTURE_HEIGHTMAP_ALGORITHMS_HLSLI_
#define _VIRTUAL_TEXTURE_HEIGHTMAP_ALGORITHMS_HLSLI_

#include "VirtualTexture.hlsli"

void get_height_and_derivatives
(
    float2 uv,
    float step,
    out float sample,
    out float2 derivatives,
    Texture2D indirectionTexture, 
    Texture2D atlasTexture,
    float virtualTextureSize,
    float indirectionTextureSize,
    virtualtexture_scale scale,
    SamplerState indirectionSampler,
    SamplerState atlasSampler
)
{
    float2 minUVSample = float2(step, step);
    float2 maxUVSample = float2(1.0 - step, 1.0f - step);
    
    // UV coordinates start the bottom left corner.
    // H5 is the height of the current texture coordinate.
    //                                  uv(1, 1)
    // ----------------------------------
    // |          |          |          |
    // |    H1    |    H2    |    H3    |
    // |          |          |          |
    // ----------------------------------
    // |          |          |          |
    // |    H4    |    H5    |    H6    |
    // |          |          |          |
    // ----------------------------------
    // |          |          |          |
    // |    H7    |    H8    |    H9    |
    // |          |          |          |
    // ----------------------------------
    // uv(0, 0)
    // The aproximation of the partial derivative is the change in height between the 
    // sides of the square formed by the adjacent heights of the the current one.
    // It can be interpreted as an equally ponderated change in heights 
    // of the 2 dimensions of the map.
    
    float2 texC1 = clamp(uv + float2(-step, step),  minUVSample, maxUVSample);
    float2 texC2 = clamp(uv + float2(0.0f, step),   minUVSample, maxUVSample);
    float2 texC3 = clamp(uv + float2(step, step),   minUVSample, maxUVSample);
    float2 texC4 = clamp(uv + float2(-step, 0.0f),  minUVSample, maxUVSample);
    float2 texC5 = clamp(uv + float2(0.0f, 0.0f),   minUVSample, maxUVSample);
    float2 texC6 = clamp(uv + float2(step, 0.0f),   minUVSample, maxUVSample);
    float2 texC7 = clamp(uv + float2(-step, -step), minUVSample, maxUVSample);
    float2 texC8 = clamp(uv + float2(0.0f, -step),  minUVSample, maxUVSample);
    float2 texC9 = clamp(uv + float2(step, -step),  minUVSample, maxUVSample);
    
    float mip1 = get_mip(texC1, virtualTextureSize, indirectionTextureSize);
    float mip2 = get_mip(texC2, virtualTextureSize, indirectionTextureSize);
    float mip3 = get_mip(texC3, virtualTextureSize, indirectionTextureSize);
    float mip4 = get_mip(texC4, virtualTextureSize, indirectionTextureSize);
    float mip5 = get_mip(texC5, virtualTextureSize, indirectionTextureSize);
    float mip6 = get_mip(texC6, virtualTextureSize, indirectionTextureSize);
    float mip7 = get_mip(texC7, virtualTextureSize, indirectionTextureSize);
    float mip8 = get_mip(texC8, virtualTextureSize, indirectionTextureSize);
    float mip9 = get_mip(texC9, virtualTextureSize, indirectionTextureSize);
    
    float2 sample1 = get_sample(texC1, mip1, indirectionTexture, indirectionTextureSize, scale, indirectionSampler);
    float2 sample2 = get_sample(texC2, mip2, indirectionTexture, indirectionTextureSize, scale, indirectionSampler);
    float2 sample3 = get_sample(texC3, mip3, indirectionTexture, indirectionTextureSize, scale, indirectionSampler);
    float2 sample4 = get_sample(texC4, mip4, indirectionTexture, indirectionTextureSize, scale, indirectionSampler);
    float2 sample5 = get_sample(texC5, mip5, indirectionTexture, indirectionTextureSize, scale, indirectionSampler);
    float2 sample6 = get_sample(texC6, mip6, indirectionTexture, indirectionTextureSize, scale, indirectionSampler);
    float2 sample7 = get_sample(texC7, mip7, indirectionTexture, indirectionTextureSize, scale, indirectionSampler);
    float2 sample8 = get_sample(texC8, mip8, indirectionTexture, indirectionTextureSize, scale, indirectionSampler);
    float2 sample9 = get_sample(texC9, mip9, indirectionTexture, indirectionTextureSize, scale, indirectionSampler);
    
    float h1 = atlasTexture.Sample(atlasSampler, sample1).a;
    float h2 = atlasTexture.Sample(atlasSampler, sample2).a;
    float h3 = atlasTexture.Sample(atlasSampler, sample3).a;
    float h4 = atlasTexture.Sample(atlasSampler, sample4).a;
    float h5 = atlasTexture.Sample(atlasSampler, sample5).a;
    float h6 = atlasTexture.Sample(atlasSampler, sample6).a;
    float h7 = atlasTexture.Sample(atlasSampler, sample7).a;
    float h8 = atlasTexture.Sample(atlasSampler, sample8).a;
    float h9 = atlasTexture.Sample(atlasSampler, sample9).a;
    
    float hx = (h3 + h6 + h9 - h1 - h4 - h7) / 6.0f;
    float hy = (h1 + h2 + h3 - h7 - h8 - h9) / 6.0f;
    
    sample = h5;
    derivatives = float2(-hx, -hy);
}

#endif
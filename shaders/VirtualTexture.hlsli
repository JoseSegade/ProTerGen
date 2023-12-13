#ifndef _VIRTUAL_TEXTURE_HLSLI_
#define _VIRTUAL_TEXTURE_HLSLI_

struct virtualtexture_scale
{
    float atlas_scale;
    float border_scale;
    float border_offset;
};

float get_inter(float mip, out float minMip, out float maxMip)
{
    minMip = floor(mip);
    maxMip = ceil(mip);
    return max(0.0f, mip - minMip);
}

float mip_level(float2 uv, float size)
{
    const float2 dx = ddx(uv * size);
    const float2 dy = ddy(uv * size);
    const float d = max(dot(dx, dx), dot(dy, dy));
    
    return max(0.5f * log2(d), 0.0f);
}

float3 sample_table(float2 uv, float mip, Texture2D indirection_texture, float indirection_size, SamplerState indirection_sampler)
{
    const float2 offset = frac(uv * indirection_size) / indirection_size;
    return indirection_texture.SampleLevel(indirection_sampler, uv - offset, mip).xyz;
}

float4 sample_atlas_bilinear
(
    float3 page,
    float2 uv,
    Texture2D atlas_texture,
    float indirection_size,
    virtualtexture_scale scale,
    SamplerState atlas_sampler
)
{
    const float mipSize = exp2(floor(page.z * 255.0 + 0.5));
    
    uv = frac(uv * indirection_size / mipSize);
    
    uv *= scale.border_scale;
    uv += scale.border_offset;
    
    const float2 offset = (floor(page.xy * 255.0 + 0.5) + uv) * scale.atlas_scale;
    
    return atlas_texture.Sample(atlas_sampler, offset);
}

float4 virtual_texture_trilinear
(
    float2 uv,
    Texture2D indirection_texture, 
    Texture2D atlas_texture, 
    float virtual_texture_size,
    float indirection_size,
    virtualtexture_scale scale,
    SamplerState indirection_sampler,
    SamplerState atlas_sampler
)
{
    float miplevel = mip_level(uv, virtual_texture_size);
    miplevel = clamp(miplevel, 0, log2(indirection_size) - 1);

    const float mip1 = floor(miplevel);
    const float mip2 = mip1 + 1;
    const float mipfrac = miplevel - mip1;

    const float3 page1 = sample_table(uv, mip1, indirection_texture, indirection_size, indirection_sampler);
    const float3 page2 = sample_table(uv, mip2, indirection_texture, indirection_size, indirection_sampler);

    const float4 sample1 = sample_atlas_bilinear(page1, uv, atlas_texture, indirection_size, scale, atlas_sampler);
    const float4 sample2 = sample_atlas_bilinear(page2, uv, atlas_texture, indirection_size, scale, atlas_sampler);

    return lerp(sample1, sample2, mipfrac);
}

float get_mip
(
    float2 uv, 
    float virtual_texture_size, 
    float indirection_size
)
{
    float mip = floor(mip_level(uv, virtual_texture_size));
    return clamp(mip, 0, log2(indirection_size));
}

float2 get_sample
(
    float2 uv, 
    float mip, 
    Texture2D indirection_texture, 
    float indirection_size, 
    virtualtexture_scale scale,
    SamplerState indirection_sampler
)
{        
    const float3 page = sample_table(uv, mip, indirection_texture, indirection_size, indirection_sampler);
    
    const float mipSize = exp2(floor(page.z * 255.0 + 0.5));
    
    uv = frac(uv * indirection_size / mipSize);
    
    uv *= scale.border_scale;
    uv += scale.border_offset;
    
    return (floor(page.xy * 255.0 + 0.5) + uv) * scale.atlas_scale;
}

#endif
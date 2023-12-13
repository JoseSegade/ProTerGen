#ifndef _NOISE_HLSLI_
#define _NOISE_HLSLI_


#ifndef VORONOI_SMOOTHNESS
#define VORONOI_SMOOTHNESS 0.2
#endif

#ifndef VORONOI_RANDOMNESS
#define VORONOI_RANDOMNESS 0.2
#endif

float2 random(float2 x)
{
    const float2 k = float2(0.3183099, 0.3678794);
    x = x * k + k.yx;
    const float2 h = frac(16.0f * k * frac(x.x * x.y * (x.x + x.y)));
    return -1.0f + 2.0f * h;
}

float2 random_seed(float2 x, float seed)
{
    const uint c = (asuint(x.x / x.y)) & uint(seed) ^ uint(0.314159 * seed);
    const float2 k = float2(0.3183099 * float(c) / seed, 0.3678794);
    x = x * k + k.yx;
    const float2 h = frac(16.0f * k * frac(x.x * x.y * (x.x + x.y)));
    return -1.0f + 2.0f * h;
}

float3 noised(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    
    float2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    float2 du = 30.0 * f * f * (f * (f - 2.0) + 1.0);
        
    float2 ga = random(i + float2(0.0, 0.0));
    float2 gb = random(i + float2(1.0, 0.0));
    float2 gc = random(i + float2(0.0, 1.0));
    float2 gd = random(i + float2(1.0, 1.0));
    
    float va = dot(ga, f - float2(0.0, 0.0));
    float vb = dot(gb, f - float2(1.0, 0.0));
    float vc = dot(gc, f - float2(0.0, 1.0));
    float vd = dot(gd, f - float2(1.0, 1.0));

    return float3(va + u.x * (vb - va) + u.y * (vc - va) + u.x * u.y * (va - vb - vc + vd),
                 ga + u.x * (gb - ga) + u.y * (gc - ga) + u.x * u.y * (ga - gb - gc + gd) +
                 du * (u.yx * (va - vb - vc + vd) + float2(vb, vc) - va));
}

float3 noised_seed(float2 p, float seed)
{
    float2 i = floor(p);
    float2 f = frac(p);
    
    float2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    float2 du = 30.0 * f * f * (f * (f - 2.0) + 1.0);
        
    float2 ga = random_seed(i + float2(0.0, 0.0), seed);
    float2 gb = random_seed(i + float2(1.0, 0.0), seed);
    float2 gc = random_seed(i + float2(0.0, 1.0), seed);
    float2 gd = random_seed(i + float2(1.0, 1.0), seed);
    
    float va = dot(ga, f - float2(0.0, 0.0));
    float vb = dot(gb, f - float2(1.0, 0.0));
    float vc = dot(gc, f - float2(0.0, 1.0));
    float vd = dot(gd, f - float2(1.0, 1.0));

    return float3(va + u.x * (vb - va) + u.y * (vc - va) + u.x * u.y * (va - vb - vc + vd),
                 ga + u.x * (gb - ga) + u.y * (gc - ga) + u.x * u.y * (ga - gb - gc + gd) +
                 du * (u.yx * (va - vb - vc + vd) + float2(vb, vc) - va));
}

float noise(float2 st)
{
    float2 i = floor(st);
    float2 f = frac(st);
    
    float a = random(i + float2(0.0, 0.0)).x;
    float b = random(i + float2(1.0, 0.0)).x;
    float c = random(i + float2(0.0, 1.0)).x;
    float d = random(i + float2(1.0, 1.0)).x;

    float2 u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);

    return lerp(a, b, u.x) +
            (c - a) * u.y * (1.0f - u.x) +
            (d - b) * u.x * u.y;
}

float3 noise3(float2 p, float seed)
{
    int seedi = asint(seed);
    return float3
    (
        abs(frac(651651.15351 * seed + 165.2354 * sin(sqrt(((p.x - 123.25895) * (p.x - 123.25895)) + ((p.y - 83.1516584) * (p.y - 83.1516584)))))),
        abs(frac(12132.36 * seedi + 5168.12 * cos(sqrt(((p.x - 748.2156) * (p.x - 748.2156)) + ((p.y - 7.2665156) * (p.y - 7.2665156)))))),
        abs(frac(150231.2518 + 153.1568 * sin(sqrt(((p.x - 30.232354) * (p.x - 30.232354)) + ((p.y - 8.656984654) * (p.y - 8.656984654))))))
    );
}

float voronoi_noise_seed(float2 p, float seed)
{
    float2 i = floor(p);
    float2 f = frac(p);
    
    float smoothness = 1.0f + 63.0 * pow(1.0f - VORONOI_SMOOTHNESS, 4.0);

    float va = 0.0f;
    float wt = 0.0f;

    [unroll]
    for (int j = -2; j <= 2; ++j)
    {
        [unroll]
        for (int i = -2; i <= 2; ++i)
        {
            float3 n = noise3(i, seed);
            
            float dx = (VORONOI_RANDOMNESS * i) - (f.x + n.x);
            float dy = (VORONOI_RANDOMNESS * j) - (f.y + n.y);

            float d = sqrt(dx * dx + dy * dy);
            float w = pow(1.0f - smoothstep(0.0f, 1.414, d), smoothness);

            va += w * n.z;
            wt += w;
        }

    }

    return va / wt;
}

float fbm_4(float2 st)
{
    float value = 0.0;
    float amplitude = 1.0;
    float frecuency = 1.0;
    float gain = 0.5;
    [unroll]
    for (int i = 0; i < 4; i++)
    {
        value += amplitude * noise(frecuency * st);
        frecuency *= 2.0;
        amplitude *= gain;
    }
    return value;
}

static const float2x2 m2 = float2x2(0.8f, -0.6f, 0.6f, 0.8f);
float fbm_terrain(float2 x)
{
    float2 p = x;
    float value = 0.0f;
    float a = 1.0;
    float gain = 8.0f;
    float frecuency = 1.0f;
    float2 d = float2(0.0f, 0.0f);
    float total = a;
    
    [unroll]
    for (int i = 0; i < 5; ++i)
    {
        const float3 n = noised(p);
        const float x = 0.5 * n.x + 1;
        d += n.yz;
        value += (a * x) / (1.0f + dot(d, d));
        a *= gain;
        gain *= 0.2f;
        frecuency *= 1.6f;
        p = frecuency * mul(p, m2);
        total += a;
    }
    
    return value / total;
}

struct FBM
{
    float Amplitude;
    float Frecuency;
    float Gain;
    uint Octaves;

};

static float2x2 m = float2x2(1.6f, 1.2f, -1.2f, 1.5);
float fbm_terrain_params(float2 x, FBM fbm, float seed)
{
    float2 p = x;
    float value = 0.0f;
    float a = 1.0f;
    float g = fbm.Amplitude;
    float f = 1.0f;
    float2 d = float2(0.0f, 0.0f);
    float total = a;
    
    for (uint i = 0; i < fbm.Octaves; ++i)
    {
        const float3 n = noised_seed(p, seed);
        const float v = 0.5 * n.x + 1;
        d += n.yz;
        value += (a * v) / (1.0f + dot(d, d));
        a *= g;
        g *= fbm.Gain;
        f *= fbm.Frecuency;
        p = mul(p * f, m);
        total += a;
    }
    
    return value / total;
}

float fbm_terrain_params_voronoi(float2 x, FBM fbm, float seed)
{
    float2 p = x;
    float value = 0.0f;
    float a = 1.0f;
    float g = fbm.Amplitude;
    float f = 1.0f;
    float2 d = float2(0.0f, 0.0f);
    float total = a;
    
    for (uint i = 0; i < fbm.Octaves; ++i)
    {
        const float3 n = voronoi_noise_seed(p, seed);
        const float x = 0.5 * n.x + 1;
        d += n.yz;
        value += (a * x) / (1.0f + dot(d, d));
        a *= g;
        g *= fbm.Gain;
        f *= fbm.Frecuency;
        p = f * mul(p, m2);
        total += a;
    }
    
    return value / total;
}

float2 hash2(float2 p)
{
    float2 h = float2(dot(p, float2(127.2, 311.7)), dot(p, float2(269.5, 183)));
    return -1.0f + 2.0f * frac(sin(h) * 43758.5453123);
}

float noise2(float2 p)
{
    const float k1 = 0.366025404;
    const float k2 = 0.211324865;
    float2 i = floor(p + (p.x + p.y) * k1);
    float2 a = p - i + (i.x + i.y) * k2;
    float2 o = a.x > a.y ? float2(1.0f, 0.0f) : float2(0.0f, 1.0f);
    float2 b = a - o + k2;
    float2 c = a - 1.0f + 2.0 * k2;
    float3 h = max(0.5 - float3(dot(a, a), dot(b, b), dot(c, c)), 0.0f);
    float3 n = h * h * h * h * float3(dot(a, hash2(i)), dot(b, hash2(i + o)), dot(c, hash2(i + 1.0f)));
    return max(0, dot(n, float3(70.0f, 70.0f, 70.0f)));
}

float fbm_terrain_params2(float2 x, FBM fbm, float seed)
{
    float value = 0.0f;
    float amplitude = fbm.Amplitude;
    float frecuency = fbm.Frecuency;
    float total = amplitude;

    for (uint i = 0; i < fbm.Octaves; ++i)
    {
        float n = amplitude * noise2(x);
        value += n;
        x = mul(x * frecuency, m);

        amplitude *= fbm.Gain;
        frecuency *= 2.0f;
        total += amplitude;
    }
    return value / total;
}

#endif
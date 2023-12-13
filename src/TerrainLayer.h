#pragma once

#include <string>
#include <stdint.h>

namespace ProTerGen
{
    static const uint32_t MAX_LAYERS = 8;
    enum class LayerDesc : uint8_t
    {
        GRADIENT = 0,
        VORONOI  = GRADIENT + 1,
        COUNT    = VORONOI + 1
    };
    static const std::string LayerDescNames[(size_t)LayerDesc::COUNT] = { "Gradient", "Voronoi" };
    struct Layer
    {
        LayerDesc layerDesc = LayerDesc::GRADIENT;
        float     amplitude = 1.0f;
        float     frecuency = 1.0f;
        float     gain      = 1.0f;
        uint32_t  octaves   = 1;
        float     seed      = 1.0f;
        float     weight    = 1.0f;
    };
    struct MaterialLayer
    {
        Material* material         = nullptr;
        float     minSlope         = 0.0f;
        float     slopeSmoothness  = 1.0f;
        float     minHeight        = 0.0f;
        float     heightSmoothness = 1.0f;
    };
}

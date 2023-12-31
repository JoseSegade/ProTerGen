#pragma once

#include <vector>
#include "CommonHeaders.h"

namespace ProTerGen
{
   inline std::vector<CD3DX12_STATIC_SAMPLER_DESC> GetStaticSamplers()
   {
      const CD3DX12_STATIC_SAMPLER_DESC pointWrap
      (
         0,
         D3D12_FILTER_MIN_MAG_MIP_POINT,
         D3D12_TEXTURE_ADDRESS_MODE_WRAP,
         D3D12_TEXTURE_ADDRESS_MODE_WRAP,
         D3D12_TEXTURE_ADDRESS_MODE_WRAP
      );

      const CD3DX12_STATIC_SAMPLER_DESC pointClamp
      (
         1,
         D3D12_FILTER_MIN_MAG_MIP_POINT,
         D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
         D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
         D3D12_TEXTURE_ADDRESS_MODE_CLAMP
      );

      const CD3DX12_STATIC_SAMPLER_DESC linearWrap
      (
         2,
         D3D12_FILTER_MIN_MAG_MIP_LINEAR,
         D3D12_TEXTURE_ADDRESS_MODE_WRAP,
         D3D12_TEXTURE_ADDRESS_MODE_WRAP,
         D3D12_TEXTURE_ADDRESS_MODE_WRAP
      );

      const CD3DX12_STATIC_SAMPLER_DESC linearClamp
      (
         3,
         D3D12_FILTER_MIN_MAG_MIP_LINEAR,
         D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
         D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
         D3D12_TEXTURE_ADDRESS_MODE_CLAMP
      );

      const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap
      (
         4,
         D3D12_FILTER_ANISOTROPIC,
         D3D12_TEXTURE_ADDRESS_MODE_WRAP,
         D3D12_TEXTURE_ADDRESS_MODE_WRAP,
         D3D12_TEXTURE_ADDRESS_MODE_WRAP,
         0.0f,
         8
      );

      const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp
      (
         5,
         D3D12_FILTER_ANISOTROPIC,
         D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
         D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
         D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
         0.0f,
         8
      );

      const CD3DX12_STATIC_SAMPLER_DESC shadow
      (
          6,
          D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
          D3D12_TEXTURE_ADDRESS_MODE_BORDER,
          D3D12_TEXTURE_ADDRESS_MODE_BORDER,
          D3D12_TEXTURE_ADDRESS_MODE_BORDER,
          0.0f,
          16,
          D3D12_COMPARISON_FUNC_LESS_EQUAL,
          D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK
      );

      return
      {
         pointWrap,
         pointClamp,
         linearWrap,
         linearClamp,
         anisotropicWrap,
         anisotropicClamp,
         shadow
      };
   }
}
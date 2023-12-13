#pragma once

#include "CommonHeaders.h"

namespace ProTerGen
{
   class TextureCreator
   {
   public:
      TextureCreator() = default;
      virtual ~TextureCreator() = default;

      void CreateFromFile
      (
         Microsoft::WRL::ComPtr<ID3D12Device> device,
         Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
         const std::wstring& path,
         Microsoft::WRL::ComPtr<ID3D12Resource>& resource,
         Microsoft::WRL::ComPtr<ID3D12Resource>& uploadResource,
         bool& isCubeMap
      ) const;
   };
}

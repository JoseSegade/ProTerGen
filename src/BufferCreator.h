#pragma once

#include "CommonHeaders.h"

namespace ProTerGen
{
   class BufferCreator
   {
   public:
      BufferCreator() = default;
      virtual ~BufferCreator() = default;

      Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer
      (
         Microsoft::WRL::ComPtr<ID3D12Device> device,
         Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList,
         const void* initData,
         size_t byteSize,
         Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer
      );

   };
}

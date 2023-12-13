#pragma once

#include "CommonHeaders.h"

namespace ProTerGen
{
   struct Texture
   {
      std::string                            Name;
      std::wstring                           Path;
      Microsoft::WRL::ComPtr<ID3D12Resource> Resource   = nullptr;
      Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
      int32_t                                Location   = -1;
   };
}

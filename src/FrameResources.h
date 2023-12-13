#pragma once

#include "CommonHeaders.h"
#include "UploadBuffer.h"
#include "ShaderConstants.h"

namespace ProTerGen
{
   class FrameResources 
   {
   public:
      FrameResources
      (
          Microsoft::WRL::ComPtr<ID3D12Device> device, 
          size_t                               passCount,
          size_t                               objectCount, 
          size_t                               materialCount,
          size_t                               shadowSplitCount,
          size_t                               terrainMatLayerCount
      );
      FrameResources(const FrameResources&) = delete;
      FrameResources& operator=(const FrameResources&) = delete;
      virtual ~FrameResources() = default;

      Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandAllocator = nullptr;

      std::unique_ptr<UploadBuffer<PassConstants>>                 PassCB                     = nullptr;
      std::unique_ptr<UploadBuffer<VirtualTextureConstants>>       VtCB                       = nullptr;
      std::unique_ptr<UploadBuffer<TerrainConstants>>              TerrainCB                  = nullptr;
      std::unique_ptr<UploadBuffer<ShadowConstants>>               ShadowCB                   = nullptr;
      std::unique_ptr<UploadBuffer<PerShadowConstants>>            PerShadowCB                = nullptr;
      std::unique_ptr<UploadBuffer<ObjectConstants>>               ObjectCB                   = nullptr;
      std::unique_ptr<UploadBuffer<MaterialConstants>>             MaterialBuffer             = nullptr;
      std::unique_ptr<UploadBuffer<ShadowSplitConstants>>          ShadowSplitBuffer          = nullptr;
      std::unique_ptr<UploadBuffer<TerrainMaterialLayerConstants>> TerrainMaterialLayerBuffer = nullptr;
      UINT64 Fence = 0;
   };
}

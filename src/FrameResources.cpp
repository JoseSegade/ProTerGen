#include "FrameResources.h"

ProTerGen::FrameResources::FrameResources
(
	Microsoft::WRL::ComPtr<ID3D12Device> device,
	size_t passCount,
	size_t objectCount,
	size_t materialCount,
	size_t shadowSplitCount,
	size_t layerTerrainMatCount
)
{
	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(CommandAllocator.ReleaseAndGetAddressOf())));

	PassCB                     = std::make_unique<UploadBuffer<PassConstants>>                (device,            passCount,  true);
	VtCB                       = std::make_unique<UploadBuffer<VirtualTextureConstants>>      (device,            passCount,  true);
	TerrainCB                  = std::make_unique<UploadBuffer<TerrainConstants>>             (device,            passCount,  true);
	ShadowCB                   = std::make_unique<UploadBuffer<ShadowConstants>>              (device,            passCount,  true);
	MaterialBuffer             = std::make_unique<UploadBuffer<MaterialConstants>>            (device,        materialCount, false);
	ObjectCB                   = std::make_unique<UploadBuffer<ObjectConstants>>              (device,          objectCount,  true);
	PerShadowCB                = std::make_unique<UploadBuffer<PerShadowConstants>>           (device,     shadowSplitCount,  true);
	ShadowSplitBuffer          = std::make_unique<UploadBuffer<ShadowSplitConstants>>         (device,     shadowSplitCount, false);
	TerrainMaterialLayerBuffer = std::make_unique<UploadBuffer<TerrainMaterialLayerConstants>>(device, layerTerrainMatCount, false);
}

#include <array>

#include "PageLoaderGpuGen.h"
#include "Config.h"

#if _DEBUG && PRINT_PERFORMANCE_TIMES
#include "Timer.h"
#endif

const char GPU_THREADS[] = "1";

ProTerGen::VT::PageGpuGen_HNC::~PageGpuGen_HNC()
{
	Dispose();
}

void ProTerGen::VT::PageGpuGen_HNC::Init
(
	PageIndexer* indexer,
	const VTDesc* info,
	std::unique_ptr<ComputeContext> computeContext,
	uint32_t tilesPerFrame
)
{
	mIndexer = indexer;
	mInfo = info;
	mComputeContext = std::move(computeContext);;
	mTilesPerDispatch = tilesPerFrame;

	// Create new compute pipeline.
	// 1 structured buffer for page settings. 
	// 1 constant buffer for terrain config -> noise and color layers.
	// 1 in/out buffer for heightmap(ubv), 8 in tex buffers (terrain color layers) and 1 out texture buffer.

	auto& device = (mComputeContext->device);
	auto& shaders = (mComputeContext->shaders);
	auto& pipeline = (mComputeContext->pipeline);
	auto& materials = (mComputeContext->materials);
	auto& descriptorHeaps = (mComputeContext->descriptorHeaps);

	// Create root signature, shaders and PSO.
	const std::string computeRootSignatureName = "PageLoaderGpuGen_RS";
	const std::vector<CD3DX12_STATIC_SAMPLER_DESC> staticSamplers =
	{
		CD3DX12_STATIC_SAMPLER_DESC
		(
			0,
			D3D12_FILTER_MIN_MAG_MIP_POINT,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP
		),
		CD3DX12_STATIC_SAMPLER_DESC
		(
			1,
			D3D12_FILTER_ANISOTROPIC,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP
		)
	};
	const std::array<std::string, 5> paramNames =
	{
		"cbHeightmapConfig", "texHeightmapOut", "texNormalOut", "texColorOut", "texColorIn"
	};
	shaders.AddConstantBufferToRootSignature(paramNames[0], computeRootSignatureName);
	shaders.AddTableToRootSignature(paramNames[1], computeRootSignatureName, 1, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, D3D12_SHADER_VISIBILITY_ALL);
	shaders.AddTableToRootSignature(paramNames[2], computeRootSignatureName, 1, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, D3D12_SHADER_VISIBILITY_ALL);
	shaders.AddTableToRootSignature(paramNames[3], computeRootSignatureName, 1, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, D3D12_SHADER_VISIBILITY_ALL);
	shaders.AddTableToRootSignature(paramNames[4], computeRootSignatureName, 8, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, D3D12_SHADER_VISIBILITY_ALL, 1);
	shaders.SetSamplersToRootSignature(computeRootSignatureName, staticSamplers);

	shaders.CompileRootSignature(device, computeRootSignatureName);

	const std::vector<std::string> shaderNames =
	{
		"ComputeTileRequest"
	};
	const std::vector<std::wstring> shaderPaths =
	{
		L".\\TileRequest.hlsl"
	};
	const std::vector<Shaders::TYPE> shaderTypes =
	{
		Shaders::TYPE::CS
	};
	const std::vector<D3D_SHADER_MACRO> shaderMacro =
	{
		D3D_SHADER_MACRO{ "TG_SIZE_X", GPU_THREADS },
		D3D_SHADER_MACRO{ "TG_SIZE_Y", GPU_THREADS },
		D3D_SHADER_MACRO{ "TG_SIZE_Z", "1" },
		D3D_SHADER_MACRO{ NULL, NULL }
	};
	shaders.CompileShaders(shaderNames, shaderPaths, shaderTypes, shaderMacro);

	const std::string computePSOName = "NoisePSO";
	shaders.CreateComputePSO(computePSOName, device, computeRootSignatureName, shaderNames.back());

	pipeline.PSOs.push_back(shaders.GetPSO(computePSOName));
	pipeline.RootSignature = shaders.GetRootSignature(computeRootSignatureName);

	pipeline.Buffers.push_back((new UploadBuffer<ComputeTileRequestConstants>(device, 1, true)));

	const std::vector<std::string> textureNamesA =
	{
		"Compute_TileRequest_Heighmap_Tex",
		"Compute_TileRequest_Normal_Tex",
		"Compute_TileRequest_Color_Tex",
	};
	const std::array<std::wstring, GenerationTextures::COUNT> textureNamesW =
	{
		L"Compute_TileRequest_Heighmap_Tex",
		L"Compute_TileRequest_Normal_Tex",
		L"Compute_TileRequest_Color_Tex",
	};
	const std::array<DXGI_FORMAT, GenerationTextures::COUNT> textureFormats = _TEXTURES_FORMAT();

	const CD3DX12_HEAP_PROPERTIES props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	const uint32_t width = mInfo->BorderedTileSize();
	const uint32_t height = mInfo->BorderedTileSize();
	D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D
	(
		DXGI_FORMAT_R32_TYPELESS,
		width,
		height,
		1,
		1,
		1,
		0,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
	);
	D3D12_RESOURCE_BARRIER barriers[GenerationTextures::COUNT] = {};
	const CD3DX12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
	for (uint32_t i = 0; i < GenerationTextures::COUNT; ++i)
	{

		Texture* texture = (materials.CreateEmptyTexture(textureNamesA[i], Materials::TextureType::TEXTURE_UAV));
		pipeline.Textures.push_back(texture);
		desc.Format = textureFormats[i];
		device->CreateCommittedResource
		(
			&props,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			nullptr,
			IID_PPV_ARGS(texture->Resource.ReleaseAndGetAddressOf())
		);
		texture->Resource->SetName(textureNamesW[i].c_str());
		barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition
		(
			texture->Resource.Get(),
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		);
		const size_t readbackBufferSize = GetRequiredIntermediateSize(texture->Resource.Get(), 0, 1);
		const CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(readbackBufferSize);
		ThrowIfFailed(device->CreateCommittedResource
		(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(texture->UploadHeap.ReleaseAndGetAddressOf())
		));
	}

	pipeline.HeapCollections.push_back("Compute_TileRequest_UAV_Heap");
	materials.InsertTexturesInSRVHeap(device, descriptorHeaps, "COMPUTE_TEXTURES", textureNamesA);

	pipeline.CommandList->ResourceBarrier(GenerationTextures::COUNT, barriers);

	mPageThread.OnRun([&](MultiPage& readState) { return LoadPage(readState); });
	mPageThread.OnComplete([&](Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, const MultiPage& rs) { OnProcessingComplete(commandList, rs); });
	mPageThread.Init();
}

void ProTerGen::VT::PageGpuGen_HNC::Dispose()
{
	mPageThread.Dispose();
}

void ProTerGen::VT::PageGpuGen_HNC::Update(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, uint32_t updateCount)
{
	mPageThread.Update(commandList, updateCount);
}

void ProTerGen::VT::PageGpuGen_HNC::Reload()
{
	mPageThread.Init();
}

void ProTerGen::VT::PageGpuGen_HNC::Submit(const Page& request)
{
	MultiPage state = {};
	state.page = request;

	mPageThread.Enqueue(state);
}

void ProTerGen::VT::PageGpuGen_HNC::Clear()
{
	Dispose();
}

void ProTerGen::VT::PageGpuGen_HNC::Restart()
{
	mPageThread.Init();
}

bool ProTerGen::VT::PageGpuGen_HNC::LoadPage(MultiPage& readState)
{	
	auto& pipeline = mComputeContext->pipeline;
	auto& descriptorHeaps = mComputeContext->descriptorHeaps;
	auto& materials = mComputeContext->materials;
	
	//ThrowIfFailed(pipeline.CommandAllocator->Reset());
	//ThrowIfFailed(pipeline.CommandList->Reset(pipeline.CommandAllocator.Get(), pipeline.PSO.Get()));

	// Constant buffer	
	const Page& page = readState.page;
	const uint32_t scaleFactor = 1 << page.Mip;
	const float mipTileSize = (float)scaleFactor * mInfo->TileSize();
	const DirectX::XMFLOAT2 min = { (float)readState.page.X * mipTileSize, (float)readState.page.Y * mipTileSize };
	const DirectX::XMFLOAT2 max = { min.x + mipTileSize, min.y + mipTileSize };
	ComputeTileRequestConstants rectCompute =
	{
		.TerrainSize = { (float)mInfo->VTSize, (float)mInfo->VTSize },
		.TileSize = { (float)mInfo->BorderedTileSize(), (float)mInfo->BorderedTileSize()},
		.Min = min,
		.Max = max,
		.Scale = 1.0f,
		.LayerCount = 1,
		.ColorTextureLayers = {},
	};
	rectCompute.ColorTextureLayers[0] =
	{
		.TextureSlot = 0,
		.ColorLayer = 0,
		.MinValue = 0.0f,
		.MaxValue = 1.0f,
	};

	auto rectComputeContants = ((UploadBuffer<ComputeTileRequestConstants>*)pipeline.Buffers.back());
	rectComputeContants->CopyData(0, rectCompute);

	// Exec
	pipeline.CommandList->SetPipelineState(pipeline.PSOs[(uint32_t)PSOs::DEFAULT].Get());
	pipeline.CommandList->SetDescriptorHeaps(1, descriptorHeaps.GetSrvHeap().Heap.GetAddressOf());
	pipeline.CommandList->SetComputeRootSignature(pipeline.RootSignature.Get());
	const uint32_t texOffset = descriptorHeaps.GetSrvTableEntryLocation("COMPUTE_TEXTURES");
	pipeline.CommandList->SetComputeRootConstantBufferView(0, rectComputeContants->Resource()->GetGPUVirtualAddress());
	pipeline.CommandList->SetComputeRootDescriptorTable(1, descriptorHeaps.GetSrvHeap().GetGPUHandle(texOffset));
	pipeline.CommandList->SetComputeRootDescriptorTable(2, descriptorHeaps.GetSrvHeap().GetGPUHandle(texOffset + 1));
	pipeline.CommandList->SetComputeRootDescriptorTable(3, descriptorHeaps.GetSrvHeap().GetGPUHandle(texOffset + 2));
	pipeline.CommandList->SetComputeRootDescriptorTable(4, descriptorHeaps.GetSrvHeap().GetGPUHandle(0));
	const uint32_t gpuGroups = mInfo->BorderedTileSize();
	pipeline.CommandList->Dispatch(gpuGroups, gpuGroups, 1);

	D3D12_RESOURCE_BARRIER barriers[GenerationTextures::COUNT] = {};
	for (uint32_t i = 0; i < GenerationTextures::COUNT; ++i)
	{
		Texture*& texture = pipeline.Textures[i];
		barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition
		(
			texture->Resource.Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_COPY_SOURCE
		);
	}
	pipeline.CommandList->ResourceBarrier(GenerationTextures::COUNT, barriers);
	const uint32_t textureSize = mInfo->BorderedTileSize();
	const std::array<uint32_t, GenerationTextures::COUNT> textureBytesPerTexel = _TEXTURES_BYTES_PER_TEXEL();
	for (uint32_t i = 0; i < GenerationTextures::COUNT; ++i)
	{
		Texture*& texture = pipeline.Textures[i];

		const D3D12_SUBRESOURCE_FOOTPRINT pitchedDesc =
		{
			.Format = texture->Resource->GetDesc().Format,
			.Width = textureSize,
			.Height = textureSize,
			.Depth = 1,
			.RowPitch = (uint32_t)Align(textureSize * textureBytesPerTexel[i], D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1)
		};

		const D3D12_PLACED_SUBRESOURCE_FOOTPRINT placedTexture2D =
		{
			.Offset = 0,
			.Footprint = pitchedDesc
		};

		const CD3DX12_TEXTURE_COPY_LOCATION dst = CD3DX12_TEXTURE_COPY_LOCATION(texture->UploadHeap.Get(), placedTexture2D);
		const CD3DX12_TEXTURE_COPY_LOCATION src = CD3DX12_TEXTURE_COPY_LOCATION(texture->Resource.Get(), 0);
		const D3D12_BOX b =
		{
			.left = 0,
			.top = 0,
			.front = 0,
			.right = textureSize,
			.bottom = textureSize,
			.back = 1
		};
		pipeline.CommandList->CopyTextureRegion(
			&dst,
			0, 0, 0,
			&src,
			&b);
		barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition
		(
			texture->Resource.Get(),
			D3D12_RESOURCE_STATE_COPY_SOURCE,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		);
	}
	pipeline.CommandList->ResourceBarrier(GenerationTextures::COUNT, barriers);
	mComputeContext->dispatchExecuteArgs(pipeline);
	
	ThrowIfFailed(pipeline.CommandAllocator->Reset());
	ThrowIfFailed(pipeline.CommandList->Reset(pipeline.CommandAllocator.Get(), pipeline.PSOs[(uint32_t)PSOs::DEFAULT].Get()));

	// Copy image to buffer
	readState.dataPtrs.resize(GenerationTextures::COUNT);
	void* data = nullptr;
	for (uint32_t i = 0; i < GenerationTextures::COUNT; ++i)
	{
		Texture*& texture = pipeline.Textures[i];
		const size_t totalSize = (size_t)textureSize * textureSize * textureBytesPerTexel[i];
		
		readState.dataPtrs[i] = malloc(totalSize);
		assert(readState.dataPtrs[i] != nullptr && "Not enough memory to complete operation");
		const D3D12_RANGE range = { .Begin = 0, .End = totalSize };
		ThrowIfFailed(texture->UploadHeap->Map(0, &range, &data));
		memcpy(readState.dataPtrs[i], data, totalSize);
		texture->UploadHeap->Unmap(0, nullptr);
	}

	if (mShowBordersEnabled)
	{
		const size_t index = (size_t)GenerationTextures::COLOR;
		ColorBorders
		(
			readState.dataPtrs[index],
			std::array<uint8_t,	TEXTURES_BYTES_PER_TEXEL()[index]>{ 0, 255, 0, 255 },
			mInfo->TileSize(),
			mInfo->BorderSize, 
			mInfo->TileSize()
		);
	}

	return true;
}

void ProTerGen::VT::PageGpuGen_HNC::OnProcessingComplete(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, const MultiPage& page)
{
	mOnLoadComplete(commandList, page);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

ProTerGen::VT::PageGpuGen_Sdh::~PageGpuGen_Sdh()
{
	Dispose();
}

void ProTerGen::VT::PageGpuGen_Sdh::Init
(
	PageIndexer* indexer,
	const VTDesc* info,
	std::unique_ptr<ComputeContext> computeContext,
	uint32_t tilesPerFrame
)
{
	mIndexer = indexer;
	mInfo = info;
	mComputeContext = std::move(computeContext);;
	mTilesPerDispatch = tilesPerFrame;

	auto& device = (mComputeContext->device);
	auto& shaders = (mComputeContext->shaders);
	auto& pipeline = (mComputeContext->pipeline);
	auto& materials = (mComputeContext->materials);
	auto& descriptorHeaps = (mComputeContext->descriptorHeaps);

	// Create root signature, shaders and PSO.
	const std::string computeRootSignatureName = "ComputeNhTileTerrainRS";
	const std::vector<CD3DX12_STATIC_SAMPLER_DESC> staticSamplers =
	{
		CD3DX12_STATIC_SAMPLER_DESC
		(
			0,
			D3D12_FILTER_MIN_MAG_MIP_POINT,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP
		),
		CD3DX12_STATIC_SAMPLER_DESC
		(
			1,
			D3D12_FILTER_ANISOTROPIC,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP
		)
	};
	const std::array<std::string, 3> paramNames =
	{
		"cbHeightmapConfig", "cbNoiseConfig", "texHeightmapOut"
	};
	shaders.AddConstantBufferToRootSignature(paramNames[0], computeRootSignatureName);
	shaders.AddConstantBufferToRootSignature(paramNames[1], computeRootSignatureName);
	shaders.AddTableToRootSignature(paramNames[2], computeRootSignatureName, 1, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, D3D12_SHADER_VISIBILITY_ALL);
	shaders.SetSamplersToRootSignature(computeRootSignatureName, staticSamplers);

	shaders.CompileRootSignature(device, computeRootSignatureName);

	const std::vector<std::wstring> shaderPaths =
	{
		gShadersPath + L"ComputeNhTileTerrain.hlsl"
	};
	const std::vector<Shaders::TYPE> shaderTypes =
	{
		Shaders::TYPE::CS
	};

	std::vector<std::string> shaderNames =
	{
		"ComputeNhTileTerrain"
	};
	std::vector<D3D_SHADER_MACRO> shaderMacro =
	{
		D3D_SHADER_MACRO{ "FBM_GRADIENT_NOISE", "" },
		D3D_SHADER_MACRO{ "TG_SIZE_X", GPU_THREADS },
		D3D_SHADER_MACRO{ "TG_SIZE_Y", GPU_THREADS },
		D3D_SHADER_MACRO{ "TG_SIZE_Z", "1" },
		D3D_SHADER_MACRO{ NULL, NULL }
	};
	std::string computePSOName = "ComputeNhTileTerrain_Height_PSO";
	shaders.CompileShaders(shaderNames, shaderPaths, shaderTypes, shaderMacro);
	shaders.CreateComputePSO(computePSOName, device, computeRootSignatureName, shaderNames.back());
	pipeline.PSOs.push_back(shaders.GetPSO(computePSOName));

	shaderNames[0] = "ComputeNhTileTerrain_SlopeDer";
	shaderMacro[0] = D3D_SHADER_MACRO{ "SLOPE_DERIVATIVES" };
	computePSOName = "ComputeNhTileTerrain_SlopeDer_PSO";
	shaders.CompileShaders(shaderNames, shaderPaths, shaderTypes, shaderMacro);
	shaders.CreateComputePSO(computePSOName, device, computeRootSignatureName, shaderNames.back());
	pipeline.PSOs.push_back(shaders.GetPSO(computePSOName));

	pipeline.RootSignature = shaders.GetRootSignature(computeRootSignatureName);

	pipeline.Buffers.push_back((new UploadBuffer<ComputeNhCB>(device, 1, true)));
	pipeline.Buffers.push_back((new UploadBuffer<ComputeNoiseCB>(device, MAX_LAYERS, true)));

	const std::vector<std::string> textureNamesA =
	{
		"Compute_TileRequest_Nh_Tex"
	};
	const std::array<std::wstring, GenerationTextures::COUNT> textureNamesW =
	{
		L"Compute_TileRequest_Nh_Tex"
	};
	const std::array<DXGI_FORMAT, GenerationTextures::COUNT> textureFormats = _TEXTURES_FORMAT();

	const CD3DX12_HEAP_PROPERTIES props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	const uint32_t width = mInfo->BorderedTileSize();
	const uint32_t height = mInfo->BorderedTileSize();
	D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D
	(
		DXGI_FORMAT_R32_TYPELESS,
		width,
		height,
		1,
		1,
		1,
		0,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
	);
	D3D12_RESOURCE_BARRIER barriers[GenerationTextures::COUNT] = {};
	const CD3DX12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
	for (uint32_t i = 0; i < GenerationTextures::COUNT; ++i)
	{
		Texture* texture = (materials.CreateEmptyTexture(textureNamesA[i], Materials::TextureType::TEXTURE_UAV));
		pipeline.Textures.push_back(texture);
		desc.Format = textureFormats[i];
		device->CreateCommittedResource
		(
			&props,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			nullptr,
			IID_PPV_ARGS(texture->Resource.ReleaseAndGetAddressOf())
		);
		texture->Resource->SetName(textureNamesW[i].c_str());
		barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition
		(
			texture->Resource.Get(),
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		);
		// We failed as a civilization when someone decided that the LAST F ROW isn't f aligned.
		//const size_t readbackBufferSize = GetRequiredIntermediateSize(texture->Resource.Get(), 0, 1);
		const size_t readbackBufferSize = Align((size_t)width * TEXTURES_BYTES_PER_TEXEL()[i], D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) * height;
		const CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(readbackBufferSize);
		ThrowIfFailed(device->CreateCommittedResource
		(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(texture->UploadHeap.ReleaseAndGetAddressOf())
		));
	}
	
	pipeline.HeapCollections.push_back("Compute_TileRequest_UAV_Heap");
	materials.InsertTexturesInSRVHeap(device, descriptorHeaps, "COMPUTE_TEXTURES", textureNamesA);

	pipeline.CommandList->ResourceBarrier(GenerationTextures::COUNT, barriers);

	mPageThread.MaxQueueSize((size_t)width * height);
	mPageThread.OnRun([&](MultiPage& readState) { return LoadPage(readState); });
	mPageThread.OnComplete([&](Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, const MultiPage& rs) { OnProcessingComplete(commandList, rs); });
	mPageThread.Init();
}

void ProTerGen::VT::PageGpuGen_Sdh::Dispose()
{
	mPageThread.Dispose();
}

void ProTerGen::VT::PageGpuGen_Sdh::Update(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, uint32_t updateCount)
{
	mPageThread.Update(commandList, updateCount);
}

void ProTerGen::VT::PageGpuGen_Sdh::Reload()
{
	mPageThread.Init();
}

void ProTerGen::VT::PageGpuGen_Sdh::Submit(const Page& request)
{
	MultiPage state = {};
	state.page = request;

	mPageThread.Enqueue(state);
}

void ProTerGen::VT::PageGpuGen_Sdh::Clear()
{
	Dispose();
}

void ProTerGen::VT::PageGpuGen_Sdh::Restart()
{
	mPageThread.Init();
}

void ProTerGen::VT::PageGpuGen_Sdh::SetLayers(const std::vector<Layer>& layers)
{
	auto& pipeline = mComputeContext->pipeline;
	auto noiseComputeConstants = ((UploadBuffer<ComputeNoiseCB>*)pipeline.Buffers[(size_t)CBs::NOISE]);
	assert(layers.size() > 0);
	for (size_t i = 0; i < layers.size() && i < MAX_LAYERS; ++i)
	{
		const Layer& l = layers[i];
		ComputeNoiseCB cncb
		{
			.amplitude   = l.amplitude,
			.frecuency   = l.frecuency,
			.gain        = l.gain,
			.octaves     = l.octaves,
			.seed        = l.seed,
			.layerWeight = l.weight,
			.index       = (uint32_t)i,
		};
		noiseComputeConstants->CopyData((uint32_t)i, cncb);
	}
	mLayerCount = ProTerGen_clamp(1, MAX_LAYERS, layers.size());
}

bool ProTerGen::VT::PageGpuGen_Sdh::LoadPage(MultiPage& readState)
{	
	auto& pipeline = mComputeContext->pipeline;
	auto& descriptorHeaps = mComputeContext->descriptorHeaps;
	auto& materials = mComputeContext->materials;
	
	// Constant buffer	
	const Page& page = readState.page;
	const uint32_t scaleFactor = 1 << page.Mip;
	const float mipTileSize = (float)scaleFactor * mInfo->TileSize();
	const float mipBorderSize = (float)scaleFactor * mInfo->BorderSize;
	const DirectX::XMFLOAT2 min = { (float)readState.page.X * mipTileSize - mipBorderSize, (float)readState.page.Y * mipTileSize - mipBorderSize };
	const DirectX::XMFLOAT2 max = { min.x + mipTileSize + 2 * mipBorderSize, min.y + mipTileSize + 2 * mipBorderSize };
	ComputeNhCB rectCompute =
	{
		.TerrainSize = { (float)mInfo->VTSize, (float)mInfo->VTSize },
		.TileSize    = { (float)mInfo->BorderedTileSize(), (float)mInfo->BorderedTileSize()},
		.Min         = min,
		.Max         = max,
		.Mip         = page.Mip,
	};
	auto rectComputeConstants = ((UploadBuffer<ComputeNhCB>*)pipeline.Buffers[(size_t)CBs::TERRAIN]);
	rectComputeConstants->CopyData(0, rectCompute);
	auto noiseComputeConstants = ((UploadBuffer<ComputeNoiseCB>*)pipeline.Buffers[(size_t)CBs::NOISE]);
	const D3D12_GPU_VIRTUAL_ADDRESS noiseGpuAddress = noiseComputeConstants->Resource()->GetGPUVirtualAddress();
	//noiseComputeConstants->CopyData(0, noiseCompute);

	// Exec
	//pipeline.CommandList->SetPipelineState(pipeline.PSOs[(uint32_t)PSOs::HEIGHT].Get());
	pipeline.CommandList->SetDescriptorHeaps(1, descriptorHeaps.GetSrvHeap().Heap.GetAddressOf());
	pipeline.CommandList->SetComputeRootSignature(pipeline.RootSignature.Get());
	const uint32_t texOffset = descriptorHeaps.GetSrvTableEntryLocation("COMPUTE_TEXTURES");
	pipeline.CommandList->SetComputeRootConstantBufferView(0, rectComputeConstants->Resource()->GetGPUVirtualAddress());
	pipeline.CommandList->SetComputeRootDescriptorTable(2, descriptorHeaps.GetSrvHeap().GetGPUHandle(texOffset));
	const uint32_t gpuGroups = mInfo->BorderedTileSize();
	for (size_t i = 0; i < mLayerCount; ++i)
	{
		pipeline.CommandList->SetPipelineState(pipeline.PSOs[(uint32_t)PSOs::HEIGHT].Get());
		pipeline.CommandList->SetComputeRootConstantBufferView(1, noiseGpuAddress + Align(sizeof(ComputeNoiseCB), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1) * i);
		pipeline.CommandList->Dispatch(gpuGroups, gpuGroups, 1);
	}	
	pipeline.CommandList->SetPipelineState(pipeline.PSOs[(uint32_t)PSOs::SLOPE_DER].Get());
	pipeline.CommandList->Dispatch(gpuGroups, gpuGroups, 1);

	D3D12_RESOURCE_BARRIER barriers[GenerationTextures::COUNT] = {};
	for (uint32_t i = 0; i < GenerationTextures::COUNT; ++i)
	{
		Texture*& texture = pipeline.Textures[i];
		barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition
		(
			texture->Resource.Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_COPY_SOURCE
		);
	}
	pipeline.CommandList->ResourceBarrier(GenerationTextures::COUNT, barriers);
	const uint32_t textureSize = mInfo->BorderedTileSize();
	const std::array<uint32_t, GenerationTextures::COUNT> textureBytesPerTexel = _TEXTURES_BYTES_PER_TEXEL();
	for (uint32_t i = 0; i < GenerationTextures::COUNT; ++i)
	{
		Texture*& texture = pipeline.Textures[i];

		const D3D12_SUBRESOURCE_FOOTPRINT pitchedDesc =
		{
			.Format = texture->Resource->GetDesc().Format,
			.Width = textureSize,
			.Height = textureSize,
			.Depth = 1,
			.RowPitch = (uint32_t)Align(textureSize * textureBytesPerTexel[i], D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1)
		};

		const D3D12_PLACED_SUBRESOURCE_FOOTPRINT placedTexture2D =
		{
			.Offset = 0,
			.Footprint = pitchedDesc
		};

		const CD3DX12_TEXTURE_COPY_LOCATION dst = CD3DX12_TEXTURE_COPY_LOCATION(texture->UploadHeap.Get(), placedTexture2D);
		const CD3DX12_TEXTURE_COPY_LOCATION src = CD3DX12_TEXTURE_COPY_LOCATION(texture->Resource.Get(), 0);
		const D3D12_BOX b =
		{
			.left = 0,
			.top = 0,
			.front = 0,
			.right = textureSize,
			.bottom = textureSize,
			.back = 1
		};
		pipeline.CommandList->CopyTextureRegion(
			&dst,
			0, 0, 0,
			&src,
			&b);
		barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition
		(
			texture->Resource.Get(),
			D3D12_RESOURCE_STATE_COPY_SOURCE,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		);
	}
	pipeline.CommandList->ResourceBarrier(GenerationTextures::COUNT, barriers);
	mComputeContext->dispatchExecuteArgs(pipeline);
	
	ThrowIfFailed(pipeline.CommandAllocator->Reset());
	ThrowIfFailed(pipeline.CommandList->Reset(pipeline.CommandAllocator.Get(), pipeline.PSOs[(uint32_t)PSOs::HEIGHT].Get()));

	// Copy image to buffer
	readState.dataPtrs.resize(GenerationTextures::COUNT);
	void* data = nullptr;
	for (uint32_t i = 0; i < GenerationTextures::COUNT; ++i)
	{
		Texture*& texture = pipeline.Textures[i];
		const size_t alignedWidth = Align((size_t)textureSize * textureBytesPerTexel[i], D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
		const size_t totalSize = alignedWidth * (textureSize);
		
		readState.dataPtrs[i] = malloc(totalSize);
		assert(readState.dataPtrs[i] != nullptr && "Not enough memory to complete operation");
		const D3D12_RANGE range = { .Begin = 0, .End = totalSize };
		ThrowIfFailed(texture->UploadHeap->Map(0, &range, &data));
		memcpy(readState.dataPtrs[i], data, totalSize);
		texture->UploadHeap->Unmap(0, nullptr);
	}

	if (mShowBordersEnabled)
	{
		const size_t index = (size_t)GenerationTextures::NORMAL_HEIGHTMAP;
		const size_t alignedRowPitch = Align((size_t)textureSize * TEXTURES_BYTES_PER_TEXEL()[index], D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
		ColorBorders
		(
			readState.dataPtrs[index],
			std::array<float, TEXTURES_BYTES_PER_TEXEL()[index] / sizeof(float)>{ 0.0f, 1.0f, 0.0f, 1.0f }, 
			mInfo->TileSize(),
			mInfo->BorderSize, 
			alignedRowPitch / sizeof(float)
		);
	}
	return true;
}

void ProTerGen::VT::PageGpuGen_Sdh::OnProcessingComplete(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, const MultiPage& page)
{
	mOnLoadComplete(commandList, page);
}

#include "CustomHeightmapTileGenerator.h"

#include <cassert>

struct ProTerGen::VT::CustomHeightmapTileGenerator::Metadata
{
	uint32_t TotalWidth = 0;
	uint32_t TotalHeight = 0;
	uint16_t TileWidth = 0;
	uint16_t TileHeight = 0;
	uint16_t FormatSize = 0;
	uint16_t IsComplete = 0;
};

struct RectComputeConstants
{
	float RectX = 0.0f;
	float RectY = 0.0f;
	float RectWidth = 0.0f;
	float RectHeight = 0.0f;
	float RectTotalWidth = 0.0f;
	float RectTotalHeight = 0.0f;
	float TileWidth = 0.0f;
	float TileHeight = 0.0f;
};

const DXGI_FORMAT HEIGHTMAP_FORMAT = DXGI_FORMAT_R16_FLOAT;
const uint32_t BYTES_PER_PIXEL = 2;

const uint32_t TEXTURE_SIZE = 4 * 1024;
const uint32_t GPU_GROUPS = 256;
const char GPU_THREADS[] = "16";

ProTerGen::VT::CustomHeightmapTileGenerator::~CustomHeightmapTileGenerator()
{
	Close();
}

bool ProTerGen::VT::CustomHeightmapTileGenerator::TryLoadCustomHeightmap(const std::wstring& filename)
{
	if (mFile.File = _wfopen(filename.c_str(), L"rb+"))
	{
		Metadata metadata = ReadFileMetadata(mFile.File);
		mFile.Offset = sizeof(Metadata);
		return metadata.IsComplete != 0;
	}
	return false;
}

void ProTerGen::VT::CustomHeightmapTileGenerator::OpenForWritting(const std::wstring& filename)
{
	if (mFile.File == nullptr)
	{
		if (!(mFile.File = _wfopen(filename.c_str(), L"wb+")))
		{
			assert(false && "Error while reading file.");
		}
	}
}

void ProTerGen::VT::CustomHeightmapTileGenerator::Init
(
	Microsoft::WRL::ComPtr<ID3D12Device> device,
	const HeightmapConfig& config,
	GpuComputePipeline& pipeline,
	Shaders& shaders,
	Materials& materials,
	DescriptorHeaps& descriptorHeaps
)
{
	mConfig = config;

	// Create root signature, shaders and PSO.
	const std::string computeRootSignatureName = "ComputeNoiseRS";
	const std::vector<CD3DX12_STATIC_SAMPLER_DESC> staticSamplers =
	{
		CD3DX12_STATIC_SAMPLER_DESC
		(
			0,
			D3D12_FILTER_MIN_MAG_MIP_POINT,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP
		)
	};
	const std::vector<std::string> paramNames =
	{
		"tableTexture", "cbImageConfig"
	};
	shaders.AddTableToRootSignature(paramNames[0], computeRootSignatureName, 1, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, D3D12_SHADER_VISIBILITY_ALL);
	shaders.AddConstantBufferToRootSignature(paramNames[1], computeRootSignatureName);
	shaders.SetSamplersToRootSignature(computeRootSignatureName, staticSamplers);

	shaders.CompileRootSignature(device, computeRootSignatureName);

	const std::vector<std::string> shaderNames =
	{
		"ComputeNoise"
	};
	const std::vector<std::wstring> shaderPaths =
	{
		L".\\ComputeNoise.hlsl"
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

	pipeline.Buffers.push_back((new UploadBuffer<RectComputeConstants>(device, 1, true)));

	// Compute UAV texture
	const std::string texName = "Compute_Tex";
	pipeline.Textures.push_back(materials.CreateEmptyTexture(texName, Materials::TextureType::TEXTURE_UAV));
	const uint32_t width = TEXTURE_SIZE;
	const uint32_t height = TEXTURE_SIZE;
	const CD3DX12_HEAP_PROPERTIES props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	const D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D
	(
		HEIGHTMAP_FORMAT,
		width,
		height,
		1,
		1,
		1,
		0,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
	);

	device->CreateCommittedResource
	(
		&props,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(pipeline.Textures.back()->Resource.ReleaseAndGetAddressOf())
	);
	pipeline.Textures.back()->Resource->SetName(L"Compute_Tex");
	std::vector<std::string> uavTextures = { texName };
	pipeline.HeapCollections.push_back("Compute_Heap");
	//materials.CreateDescriptorHeapCollection(device, pipeline.HeapCollections.back(), descriptorHeaps, uavTextures);

	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition
	(
		pipeline.Textures.back()->Resource.Get(),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);
	pipeline.CommandList->ResourceBarrier(1, &barrier);

	const size_t readbackBufferSize = GetRequiredIntermediateSize(pipeline.Textures.back()->Resource.Get(), 0, 1);

	CD3DX12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
	CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(readbackBufferSize);
	ThrowIfFailed(device->CreateCommittedResource
	(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(pipeline.Textures.back()->UploadHeap.ReleaseAndGetAddressOf())
	));
}

void ProTerGen::VT::CustomHeightmapTileGenerator::StartThread
(
	const std::function<void(void)>& executePipeline,
	GpuComputePipeline& pipeline,
	Shaders& shaders,
	Materials& materials,
	DescriptorHeaps& descriptorHeaps
)
{
	mRun.store(true);
	mExecutePipeline = executePipeline;
	mThread = std::jthread([&]() { Run(pipeline, shaders, materials, descriptorHeaps); });
}

void ProTerGen::VT::CustomHeightmapTileGenerator::Abort()
{
}

void ProTerGen::VT::CustomHeightmapTileGenerator::Close()
{
	if (mFile.File != nullptr)
	{
		fclose(mFile.File);
		mFile.File = nullptr;
	}
}

ProTerGen::VT::CustomHeightmapTileGenerator::Metadata ProTerGen::VT::CustomHeightmapTileGenerator::ReadFileMetadata(FILE*& file) const
{
	Metadata out = {};

	if (fread(&out, sizeof(out), 1, file) != 1) return Metadata{};

	return out;
}

void ProTerGen::VT::CustomHeightmapTileGenerator::WriteFileMetadata(FILE*& file, Metadata metadata) const
{
	_fseeki64(file, 0, SEEK_SET);
	fwrite(&metadata, sizeof(metadata), 1, file);
}

void ProTerGen::VT::CustomHeightmapTileGenerator::WriteIscompleteMetadata(FILE* file, bool isComplete) const
{
	const size_t offset = sizeof(Metadata) - sizeof(uint16_t);
	_fseeki64(file, offset, SEEK_SET);
	putc(1, file);
}

void ProTerGen::VT::CustomHeightmapTileGenerator::Run
(
	GpuComputePipeline& pipeline,
	Shaders& shaders,
	Materials& materials,
	DescriptorHeaps& descriptorHeaps
)
{
	Metadata m =
	{
		.TotalWidth = mConfig.Width,
		.TotalHeight = mConfig.Height,
		.TileWidth = (uint16_t)mConfig.TileWidth,
		.TileHeight = (uint16_t)mConfig.TileHeight,
		.FormatSize = (uint16_t)BYTES_PER_PIXEL,
		.IsComplete = (uint16_t)0
	};
	WriteFileMetadata(mFile.File, m);

	// TODO: iterations is a hardcoded variable: caution
	const size_t iterations = 1;
	for (uint32_t i = 0; i < iterations && mRun; ++i)
	{
		// Constant buffer	
		const RectComputeConstants rectCompute =
		{
			.RectX = (float)((i * TEXTURE_SIZE) % mConfig.Width),
			.RectY = (float)((i * TEXTURE_SIZE) / mConfig.Width),
			.RectWidth = (float)TEXTURE_SIZE,
			.RectHeight = (float)TEXTURE_SIZE,
			.RectTotalWidth = (float)mConfig.Width,
			.RectTotalHeight = (float)mConfig.Height,
			.TileWidth = (float)mConfig.TileWidth,
			.TileHeight = (float)mConfig.TileHeight
		};

		auto rectComputeContants = ((UploadBuffer<RectComputeConstants>*)pipeline.Buffers.back());
		rectComputeContants->CopyData(0, rectCompute);

		// Exec
		pipeline.CommandList->SetPipelineState(pipeline.PSOs[PSOs::DEFAULT].Get());
		pipeline.CommandList->SetDescriptorHeaps(1, descriptorHeaps.GetSrvHeap().Heap.GetAddressOf());
		pipeline.CommandList->SetComputeRootSignature(pipeline.RootSignature.Get());
		//const uint32_t texOffset = materials.GetCollectionLocation(pipeline.HeapCollections.back());
		//pipeline.CommandList->SetComputeRootDescriptorTable(0, descriptorHeaps.GetSrvHeap().GetGPUHandle(texOffset));
		pipeline.CommandList->SetComputeRootConstantBufferView(1, rectComputeContants->Resource()->GetGPUVirtualAddress());
		pipeline.CommandList->Dispatch(GPU_GROUPS, GPU_GROUPS, 1);

		Texture*& texture = pipeline.Textures.back();
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition
		(
			texture->Resource.Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_COPY_SOURCE
		);
		pipeline.CommandList->ResourceBarrier(1, &barrier);
		const size_t totalSize = (size_t)TEXTURE_SIZE * TEXTURE_SIZE * BYTES_PER_PIXEL;
		{
			// copy image to buffer
			const D3D12_SUBRESOURCE_FOOTPRINT pitchedDesc =
			{
				.Format = texture->Resource->GetDesc().Format,
				.Width = TEXTURE_SIZE,
				.Height = TEXTURE_SIZE,
				.Depth = 1,
				.RowPitch = (uint32_t)Align(TEXTURE_SIZE * BYTES_PER_PIXEL, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1)
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
				.right = TEXTURE_SIZE,
				.bottom = TEXTURE_SIZE,
				.back = 1
			};
			pipeline.CommandList->CopyTextureRegion(
				&dst,
				0, 0, 0,
				&src,
				&b);
		}
		barrier = CD3DX12_RESOURCE_BARRIER::Transition
		(
			texture->Resource.Get(),
			D3D12_RESOURCE_STATE_COPY_SOURCE,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		);
		pipeline.CommandList->ResourceBarrier(1, &barrier);
		mExecutePipeline();

		// Copy image to file
		void* data = nullptr;
		const D3D12_RANGE range = { .Begin = 0, .End = totalSize };
		ThrowIfFailed(texture->UploadHeap->Map(0, &range, &data));
		if (fwrite(data, 1, totalSize, mFile.File) != totalSize)
		{
			printf("Error while writting file.");
		}
		texture->UploadHeap->Unmap(0, nullptr);

	}
	WriteIscompleteMetadata(mFile.File, true);
}

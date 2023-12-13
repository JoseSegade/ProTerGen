#include "GrassCompute.h"
#include "Config.h"
#include "MathHelpers.h"


void ProTerGen::GrassCompute::Init
(
	Microsoft::WRL::ComPtr<ID3D12Device> device,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
	Shaders& shaders,
	DescriptorHeaps& descriptorHeap,
	GrassComputeSettings settings
)
{
	mSettings = settings;
	const uint32_t xDim = 1024;
	const uint32_t yDim = 1024;
	{
		mGrassCB = std::make_unique<UploadBuffer<GrassCB>>(device, 1, true);
	}
	{
		const size_t size = sizeof(DirectX::XMFLOAT3) * 256 * 256;
		const D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		const D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
		ThrowIfFailed(device->CreateCommittedResource
		(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(mPool.ReleaseAndGetAddressOf())
		));
		mPool->SetName(L"GrassCompute_Pool");

		const D3D12_HEAP_PROPERTIES upHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		const D3D12_RESOURCE_DESC upResDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
		ThrowIfFailed(device->CreateCommittedResource
		(
			&upHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&upResDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(mPoolUp.ReleaseAndGetAddressOf())
		));
		mPoolUp->SetName(L"GrassCompute_Pool_Update");

		void* mapPtr = nullptr;
		ThrowIfFailed(mPoolUp->Map(0, nullptr, &mapPtr));
		for (size_t i = 0; i < (size / sizeof(DirectX::XMFLOAT3)); ++i)
		{
			const float x = Map(0.0f, (float)RAND_MAX, 0.0f, 1.0f, (float)rand());
			const float z = Map(0.0f, (float)RAND_MAX, 0.0f, 1.0f, (float)rand());
			((DirectX::XMFLOAT3*)mapPtr)[i] = { x, 0.0f, z };
		}
		mPoolUp->Unmap(0, nullptr);
		mapPtr = nullptr;

		const D3D12_RESOURCE_BARRIER barriersCopy[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(mPool.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST, 0),
		};
		commandList->ResourceBarrier(_countof(barriersCopy), barriersCopy);
		commandList->CopyBufferRegion(mPool.Get(), 0, mPoolUp.Get(), 0, size);
		const D3D12_RESOURCE_BARRIER barriersDone[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(mPool.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, 0),
		};
		commandList->ResourceBarrier(_countof(barriersDone), barriersDone);
	}
	{
		const uint32_t size = sizeof(DirectX::XMFLOAT3) * xDim * yDim;
		const D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		const D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		ThrowIfFailed(device->CreateCommittedResource
		(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(mGrass.ReleaseAndGetAddressOf())
		));
		mPool->SetName(L"GrassCompute_Grass_Buffer");
	}
	{
		const uint32_t size = sizeof(uint32_t);
		const D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		const D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		ThrowIfFailed(device->CreateCommittedResource
		(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(mSize.ReleaseAndGetAddressOf())
		));
		mSize->SetName(L"GrassCompute_Size_Buffer");
	}
	{
		const std::vector<std::string> names =
		{
			"GrassGenCompute"
		};
		const std::vector<std::wstring> paths =
		{
			gShadersPath + L"GrassGenCompute.hlsl"
		};
		const std::vector<Shaders::TYPE> types =
		{
			Shaders::TYPE::CS
		};
		const std::vector<D3D_SHADER_MACRO> defines =
		{
			{ "TG_SIZE_X", "1"},
			{ "TG_SIZE_Y", "1"},
			{ "TG_SIZE_Z", "1"},
			{ "GROUP_WIDTH", "1024" },
			{ NULL, NULL }
		};

		shaders.CompileShaders(names, paths, types, defines);
	}
	{
		shaders.AddConstantBufferToRootSignature("grass_cbv", "grass_compute_rs", 0);
		shaders.AddTableToRootSignature("grass_compute_grass", "grass_compute_rs", 1, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, D3D12_SHADER_VISIBILITY_ALL);
		shaders.AddTableToRootSignature("grass_compute_size", "grass_compute_rs", 1, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, D3D12_SHADER_VISIBILITY_ALL);
		shaders.AddTableToRootSignature("grass_compute_pool", "grass_compute_rs", 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, D3D12_SHADER_VISIBILITY_ALL, 1);
		shaders.CompileRootSignature(device, "grass_compute_rs");
	}
	{
		//mPoolIdx = descriptorHeap.SetSrvIndex("grass_compute_pool");
		//mGrassIdx = descriptorHeap.SetSrvIndex("grass_compute_grass");
		//mSizeIdx = descriptorHeap.SetSrvIndex("grass_compute_size");
	}
	{
		const D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap.GetSrvHeap().GetCPUHandle(mPoolIdx);
		const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc =
		{
			.Format = DXGI_FORMAT_UNKNOWN,
			.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
			.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
			.Buffer =
			{
				.FirstElement = 0,
				.NumElements = 256 * 256,
				.StructureByteStride = sizeof(DirectX::XMFLOAT3),
				.Flags = D3D12_BUFFER_SRV_FLAG_NONE
			}
		};
		device->CreateShaderResourceView(mPool.Get(), &srvDesc, handle);
	}
	{
		const D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap.GetSrvHeap().GetCPUHandle(mGrassIdx);
		const D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc =
		{
			.Format = DXGI_FORMAT_UNKNOWN,
			.ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
			.Buffer =
			{
				.FirstElement = 0,
				.NumElements = 1024 * 1024,
				.StructureByteStride = sizeof(DirectX::XMFLOAT3),
				.CounterOffsetInBytes = 0,
				.Flags = D3D12_BUFFER_UAV_FLAG_NONE
			}
		};
		device->CreateUnorderedAccessView(mGrass.Get(), nullptr, &uavDesc, handle);
	}
	{
		const D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap.GetSrvHeap().GetCPUHandle(mSizeIdx);
		const D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc =
		{
			.Format = DXGI_FORMAT_UNKNOWN,
			.ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
			.Buffer = 
			{
				.FirstElement = 0,
				.NumElements = 1,
				.StructureByteStride = sizeof(uint32_t),
				.CounterOffsetInBytes = 0,
				.Flags = D3D12_BUFFER_UAV_FLAG_NONE
			}
		};
		device->CreateUnorderedAccessView(mSize.Get(), nullptr, &uavDesc, handle);
	}
	{
		shaders.CreateComputePSO("grassComputePSO", device, "grass_compute_rs", "GrassGenCompute");
		shaders.FreeShadersMemory();
	}
}

void ProTerGen::GrassCompute::Compute
(
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList,
	Shaders& shaders,
	DescriptorHeaps& descriptorHeaps,
	const DirectX::XMFLOAT3& eyePos
)
{
	const uint32_t xDim = 1024;
	const uint32_t yDim = 1024;
	cmdList->SetComputeRootSignature(shaders.GetRootSignature("grass_compute_rs").Get());
	cmdList->SetPipelineState(shaders.GetPSO("grassComputePSO").Get());
	uint32_t i = 0;
	const GrassCB gcb
	{
		.eyePos = eyePos,
		.distance = 100.0f,
		.grassPoolSize = 256 * 256
	};
	mGrassCB->CopyData(0, gcb);
	cmdList->SetComputeRootConstantBufferView(i++, mGrassCB->Resource()->GetGPUVirtualAddress());
	cmdList->SetComputeRootDescriptorTable(i++, descriptorHeaps.GetSrvHeap().GetGPUHandle(mGrassIdx));
	cmdList->SetComputeRootDescriptorTable(i++, descriptorHeaps.GetSrvHeap().GetGPUHandle(mSizeIdx));
	cmdList->SetComputeRootDescriptorTable(i++, descriptorHeaps.GetSrvHeap().GetGPUHandle(mPoolIdx));
	cmdList->Dispatch(1024, 1024, 1);

	D3D12_RESOURCE_BARRIER barriers[] =
	{
		CD3DX12_RESOURCE_BARRIER::Transition(mGrass.Get(), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, 0),
	};
	cmdList->ResourceBarrier(_countof(barriers), barriers);
}


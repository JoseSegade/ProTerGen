#include "ParticleSystem.h"
#include "RenderSystem.h"
#include "MathHelpers.h"

void ProTerGen::StaticGrassParticleSystem::Init()
{
	for (ECS::Entity entity : mEntities)
	{
		mMeshes.CreateNewMeshGpu(BuildUniqueId(entity, 0));
	}
}

void ProTerGen::StaticGrassParticleSystem::Update(double delta)
{
	for (ECS::Entity entity : mEntities)
	{
		GrassParticleComponent& gpc = mRegister->GetComponent<GrassParticleComponent>(entity);
		if (gpc.Dirty)
		{
			const std::string meshId = BuildUniqueId(entity, 0);
			MeshGpu& meshGpu = mMeshes.GetMeshGpu(meshId);
			void* vertices = nullptr;
			ThrowIfFailed(meshGpu.VertexBufferUploader->Map(0, nullptr, &vertices));
			memcpy(vertices, gpc.ParticlePositions.data(), gpc.ParticlePositions.size() * sizeof(DirectX::XMFLOAT3));
			meshGpu.VertexBufferUploader->Unmap(0, nullptr);
			vertices = nullptr;
		}
	}
}

void ProTerGen::StaticGrassParticleSystem::CreateOnGpu(Microsoft::WRL::ComPtr<ID3D12Device> device, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList)
{
	for (ECS::Entity entity : mEntities)
	{
		const std::string meshId = BuildUniqueId(entity, 0);
		MeshGpu& meshGpu = mMeshes.GetMeshGpu(meshId);
		MeshRendererComponent& mrc = mRegister->GetComponent<MeshRendererComponent>(entity);
		GrassParticleComponent& gpc = mRegister->GetComponent<GrassParticleComponent>(entity);
		
		{
			// Index buffer
			const size_t indexMaxSize = sizeof(Index) * (gpc.MaxSize);
			CD3DX12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
			CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(indexMaxSize);
			ThrowIfFailed(device->CreateCommittedResource
			(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_COMMON,
				nullptr,
				IID_PPV_ARGS(meshGpu.IndexBufferGPU.ReleaseAndGetAddressOf())
			));
			meshGpu.IndexBufferGPU->SetName((L"ParticleSystem_IndexBuffer_" + std::to_wstring(entity)).c_str());
			heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			ThrowIfFailed(device->CreateCommittedResource
			(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(meshGpu.IndexBufferUploader.ReleaseAndGetAddressOf())
			));
			meshGpu.IndexBufferUploader->SetName((L"ParticleSystem_IndexBufferUpload_" + std::to_wstring(entity)).c_str());

			void* pIndices = nullptr;
			Index* indices = (Index*)malloc(indexMaxSize);
			if (indices == nullptr)
			{
				printf("Not enough memory while reserving index buffer for particle system at entity %lu", (uint32_t)entity);
				throw - 1;
			}
			for (size_t i = 0; i < gpc.MaxSize; ++i)
			{
				indices[i] = (Index)i;
			}
			ThrowIfFailed(meshGpu.IndexBufferUploader->Map(0, nullptr, &pIndices));
			memcpy(pIndices, indices, indexMaxSize);
			meshGpu.IndexBufferUploader->Unmap(0, nullptr);
			pIndices = nullptr;
			free(indices);
			indices = nullptr;

			meshGpu.IndexBufferByteSize = sizeof(Index) * (gpc.ParticlePositions.size());
			meshGpu.SubMesh[""].IndexCount = gpc.ParticlePositions.size();
		}

		{
			CD3DX12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
			CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(DirectX::XMFLOAT3) * gpc.MaxSize);
			ThrowIfFailed(device->CreateCommittedResource
			(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_COMMON,
				nullptr,
				IID_PPV_ARGS(meshGpu.VertexBufferGPU.ReleaseAndGetAddressOf())
			));
			meshGpu.VertexBufferGPU->SetName((L"ParticleSystem_VertexBuffer_" + std::to_wstring(entity)).c_str());

			heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			ThrowIfFailed(device->CreateCommittedResource
			(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(meshGpu.VertexBufferUploader.ReleaseAndGetAddressOf())
			));
			meshGpu.VertexBufferUploader->SetName((L"ParticleSystem_VertexBufferUpload_" + std::to_wstring(entity)).c_str());

			void* vertices = nullptr;
			ThrowIfFailed(meshGpu.VertexBufferUploader->Map(0, nullptr, &vertices));
			memcpy(vertices, gpc.ParticlePositions.data(), gpc.ParticlePositions.size() * sizeof(DirectX::XMFLOAT3));
			meshGpu.VertexBufferUploader->Unmap(0, nullptr);
			vertices = nullptr;

			meshGpu.VertexBufferByteSize = sizeof(DirectX::XMFLOAT3) * (gpc.ParticlePositions.size());
			meshGpu.VertexByteStride = sizeof(DirectX::XMFLOAT3);
		}

		CD3DX12_RESOURCE_BARRIER barriers[2] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(meshGpu.IndexBufferGPU.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST),
			CD3DX12_RESOURCE_BARRIER::Transition(meshGpu.VertexBufferGPU.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST),
		};
		cmdList->ResourceBarrier(_countof(barriers), barriers);
		cmdList->CopyBufferRegion(meshGpu.IndexBufferGPU.Get(), 0, meshGpu.IndexBufferUploader.Get(), 0, meshGpu.IndexBufferByteSize);
		cmdList->CopyBufferRegion(meshGpu.VertexBufferGPU.Get(), 0, meshGpu.VertexBufferUploader.Get(), 0, meshGpu.VertexBufferByteSize);
		barriers[0] =
			CD3DX12_RESOURCE_BARRIER::Transition(meshGpu.IndexBufferGPU.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
		barriers[1] =
			CD3DX12_RESOURCE_BARRIER::Transition(meshGpu.VertexBufferGPU.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		cmdList->ResourceBarrier(_countof(barriers), barriers);

		// NOTE: upload buffers are pending to be released... It depends if it will have further copy ops.
		gpc.Dirty = false;
		gpc.CreatedOnGpu = true;
		mrc.MeshGpuLocation = meshId;
	}
}

void ProTerGen::StaticGrassParticleSystem::UpdateOnGpu(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList)
{
	for (ECS::Entity entity : mEntities)
	{
		GrassParticleComponent& gpc = mRegister->GetComponent<GrassParticleComponent>(entity);
		if (gpc.Dirty)
		{
			MeshGpu& meshGpu = mMeshes.GetMeshGpu(BuildUniqueId(entity, 0));
			CD3DX12_RESOURCE_BARRIER barriers[1] =
			{
				CD3DX12_RESOURCE_BARRIER::Transition(meshGpu.VertexBufferGPU.Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST),
			};
			cmdList->ResourceBarrier(_countof(barriers), barriers);
			cmdList->CopyBufferRegion(meshGpu.VertexBufferGPU.Get(), 0, meshGpu.VertexBufferUploader.Get(), 0, sizeof(DirectX::XMFLOAT3) * gpc.ParticlePositions.size());
			barriers[0] =
				CD3DX12_RESOURCE_BARRIER::Transition(meshGpu.VertexBufferGPU.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
			cmdList->ResourceBarrier(_countof(barriers), barriers);

			meshGpu.IndexBufferByteSize = sizeof(Index) * gpc.ParticlePositions.size();
			meshGpu.VertexBufferByteSize = sizeof(DirectX::XMFLOAT3) * gpc.ParticlePositions.size();
			meshGpu.SubMesh[""].IndexCount = gpc.ParticlePositions.size();

			gpc.Dirty = false;
		}
	}
}

void ProTerGen::DynamicGrassParticleSystem::Init()
{
	for (ECS::Entity entity : mEntities)
	{
		for (uint32_t i = 0; i < gNumFrames; ++i)
		{
			mMeshes.CreateNewMeshGpu(BuildUniqueId(entity, i));
		}
	}
}

void ProTerGen::DynamicGrassParticleSystem::Update(double dt)
{
}

void ProTerGen::DynamicGrassParticleSystem::UpdateOnGpu(Microsoft::WRL::ComPtr<ID3D12Device> device, uint32_t currentFrame)
{
	for (ECS::Entity entity : mEntities)
	{
		DynamicParticleComponent& dpc = mRegister->GetComponent<DynamicParticleComponent>(entity);
		MeshRendererComponent& mrc = mRegister->GetComponent<MeshRendererComponent>(entity);
		const std::string id = BuildUniqueId(entity, currentFrame);
		MeshGpu& meshGpu = mMeshes.GetMeshGpu(id);

		if (dpc.ParticlePositions.size() == 0) dpc.ParticlePositions.push_back({ 0.0f, 0.0f, 0.0f });

		const D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(dpc.ParticlePositions.size() * sizeof(Index));
		ThrowIfFailed(device->CreateCommittedResource
		(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(meshGpu.IndexBufferGPU.ReleaseAndGetAddressOf())
		));
		meshGpu.IndexBufferGPU->SetName((L"DynamicGrassParticleSystem_IndexBuffer_" + std::to_wstring(entity)).c_str());
		resDesc = CD3DX12_RESOURCE_DESC::Buffer(dpc.ParticlePositions.size() * sizeof(DirectX::XMFLOAT3));
		ThrowIfFailed(device->CreateCommittedResource
		(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(meshGpu.VertexBufferGPU.ReleaseAndGetAddressOf())
		));
		meshGpu.VertexBufferGPU->SetName((L"DynamicGrassParticleSystem_VertexBuffer_" + std::to_wstring(entity)).c_str());

		void* dstI = nullptr;
		ThrowIfFailed(meshGpu.IndexBufferGPU->Map(0, nullptr, &dstI));
		for (size_t i = 0; i < dpc.ParticlePositions.size(); ++i)
		{
			((Index*)dstI)[i] = (Index)i;
		}
		meshGpu.IndexBufferGPU->Unmap(0, nullptr);
		dstI = nullptr;

		void* dstV = nullptr;
		ThrowIfFailed(meshGpu.VertexBufferGPU->Map(0, nullptr, &dstV));
		memcpy(dstV, dpc.ParticlePositions.data(), dpc.ParticlePositions.size() * sizeof(DirectX::XMFLOAT3));
		meshGpu.VertexBufferGPU->Unmap(0, nullptr);
		dstV = nullptr;

		meshGpu.IndexBufferByteSize = dpc.ParticlePositions.size() * sizeof(Index);
		meshGpu.VertexBufferByteSize = dpc.ParticlePositions.size() * sizeof(DirectX::XMFLOAT3);
		meshGpu.VertexByteStride = sizeof(DirectX::XMFLOAT3);
		meshGpu.SubMesh[""].IndexCount = dpc.ParticlePositions.size();

		mrc.MeshGpuLocation = id;

		dpc.ParticlePositions.clear();
	}
}
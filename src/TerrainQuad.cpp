#include "TerrainQuad.h"
#include <DirectXMath.h>
#include "Config.h"
#include "MathHelpers.h"
#include "RenderSystem.h"

void ProTerGen::TerrainQTQuadSystem::Init()
{
	for (const ECS::Entity& entity : mEntities)
	{
		for (uint32_t i = 0; i < gNumFrames; ++i)
		{
			mMeshes->CreateNewMeshGpu(BuildUniqueId(entity, i));

		}
	}
}

void ProTerGen::TerrainQTQuadSystem::Update(double dt)
{
	/*
	const DirectX::XMFLOAT3&        camPos     = mCamera->Position;
	const DirectX::BoundingFrustum& camFrustum = mCamera->Frustum;
	// Iterate qt.
	for (const ECS::Entity& entity : mEntities)
	{
		TerrainQTQuadComponent& tc = mRegister->GetComponent<TerrainQTQuadComponent>(entity);
			
	}
	*/
}

void ProTerGen::TerrainQTQuadSystem::UpdateOnGpu(Microsoft::WRL::ComPtr<ID3D12Device> device, uint32_t currentFrame)
{
	for (const ECS::Entity& entity : mEntities)
	{
		const std::string       meshName = BuildUniqueId(entity, currentFrame);
		TerrainQTQuadComponent& tc       = mRegister->GetComponent<TerrainQTQuadComponent>(entity);
		MeshRendererComponent&  mrc      = mRegister->GetComponent<MeshRendererComponent>(entity);
		MeshGpu&                meshGpu  = mMeshes->GetMeshGpu(meshName);
		Mesh                    mesh{};

		const uint32_t numChunks        = 1 << (tc.chunksPerSideExp);
		const float    shalfSide        = -(tc.side * 0.5f);
		const float    chunkSize        = tc.side / (float)numChunks;
		const float    hchunkSize       = chunkSize * 0.5f;
		const float    chunkPointStride = chunkSize / tc.numCpuDivisions;
		const float    invSide          = 1.0f / tc.side;
		Index vertexIndex = 0;
		const float cx = 0.0f;
		const float cy = 0.0f;
		for (uint32_t chunkY = 0; chunkY < numChunks; ++chunkY)
		{
			for (uint32_t chunkX = 0; chunkX < numChunks; ++chunkX)
			{
				for (uint32_t subdivY = 0; subdivY < tc.numCpuDivisions; ++subdivY)
				{
					for (uint32_t subdivX = 0; subdivX < tc.numCpuDivisions; ++subdivX)
					{
						const float xo = hchunkSize + chunkX * chunkSize + subdivX * chunkPointStride;
						const float yo = hchunkSize + chunkY * chunkSize + subdivY * chunkPointStride;
						const float dx = (shalfSide + xo) - cx;
						const float dy = (shalfSide + yo) - cy;
						const float d = std::sqrt((dx * dx) + (dy * dy));
						Vertex v
						{
							.Position { shalfSide + xo, 0.0f, shalfSide + yo, d },
							.Normal   { 0.0f, 1.0f, 0.0f },
							.TexC     { xo * invSide, yo * invSide },
							.TangentU { 0.0f, 0.0f, 1.0f }
						};
						mesh.Vertices.push_back(v);
						mesh.Indices.push_back(vertexIndex++);
					}
				}
				
			}
		}

		const size_t                vVerticesSize = sizeof(Vertex) * mesh.Vertices.size();
		const D3D12_HEAP_PROPERTIES vHeapProps    = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		const D3D12_RESOURCE_DESC   vResDesc      = CD3DX12_RESOURCE_DESC::Buffer(vVerticesSize, D3D12_RESOURCE_FLAG_NONE);
		ThrowIfFailed(device->CreateCommittedResource
		(
			&vHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&vResDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(meshGpu.VertexBufferGPU.ReleaseAndGetAddressOf())
		));
		meshGpu.VertexBufferGPU->SetName((L"TERRAIN_VB_" + std::to_wstring(currentFrame)).c_str());
		void* vCopyPtr = nullptr;
		ThrowIfFailed(meshGpu.VertexBufferGPU->Map(0, nullptr, &vCopyPtr));
		memcpy(vCopyPtr, mesh.Vertices.data(), vVerticesSize);
		meshGpu.VertexBufferGPU->Unmap(0, nullptr);
		vCopyPtr = nullptr;

		const size_t                iIndicesSize = sizeof(Index) * mesh.Indices.size();
		const D3D12_HEAP_PROPERTIES iHeapProps   = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		const D3D12_RESOURCE_DESC   iResDesc     = CD3DX12_RESOURCE_DESC::Buffer(iIndicesSize, D3D12_RESOURCE_FLAG_NONE);
		ThrowIfFailed(device->CreateCommittedResource
		(
			&iHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&iResDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(meshGpu.IndexBufferGPU.ReleaseAndGetAddressOf())
		));
		meshGpu.IndexBufferGPU->SetName((L"TERRAIN_IB_" + std::to_wstring(currentFrame)).c_str());
		void* iCopyPtr = nullptr;
		ThrowIfFailed(meshGpu.IndexBufferGPU->Map(0, nullptr, &iCopyPtr));
		memcpy(iCopyPtr, mesh.Indices.data(), iIndicesSize);
		meshGpu.IndexBufferGPU->Unmap(0, nullptr);
		iCopyPtr = nullptr;

		meshGpu.VertexByteStride       = sizeof(Vertex);
		meshGpu.VertexBufferByteSize   = vVerticesSize;
		meshGpu.IndexBufferByteSize    = iIndicesSize;
		meshGpu.IndexFormat            = DXGI_FORMAT_R32_UINT;
		meshGpu.SubMesh[""].IndexCount = mesh.Vertices.size();

		mrc.MeshGpuLocation            = meshName;
	}
}

#include "TerrainTree.h"
#include "Config.h"
#include "MathHelpers.h"
#include "ParticleSystem.h"
#include "Noiser.h"
#if _DEBUG && PRINT_PERFORMANCE_TIMES
#include "Timer.h"
#endif

const uint32_t NUM_VERTICES_PER_MINIMAL_PATCH_SIDE = 3;
const uint32_t NUM_VERTICES_PER_MINIMAL_PATCH = NUM_VERTICES_PER_MINIMAL_PATCH_SIDE * NUM_VERTICES_PER_MINIMAL_PATCH_SIDE;

constexpr uint32_t ComputeMipIncrement(uint32_t index, ProTerGen::RQuadTreeTerrain::Border border, ProTerGen::RQuadTreeTerrain::Corner corner) 
{
	uint32_t increment = 0;
	if      (border == (ProTerGen::RQuadTreeTerrain::Border::NORTH)     && (index == 6 || index == 7)) ++increment;
	else if (border == (ProTerGen::RQuadTreeTerrain::Border::SOUTH)     && (index == 0 || index == 1)) ++increment;
	else if (border == (ProTerGen::RQuadTreeTerrain::Border::EAST)      && (index == 7 || index == 2)) ++increment;
	else if (border == (ProTerGen::RQuadTreeTerrain::Border::WEST)      && (index == 0 || index == 5)) ++increment;
	else if (border == (ProTerGen::RQuadTreeTerrain::Border::NORTHEAST) && (index == 2 || index == 5 || index == 6)) ++increment;
	else if (border == (ProTerGen::RQuadTreeTerrain::Border::NORTHWEST) && (index == 0 || index == 5 || index == 6)) ++increment;
	else if (border == (ProTerGen::RQuadTreeTerrain::Border::SOUTHEAST) && (index == 0 || index == 1 || index == 6)) ++increment;
	else if (border == (ProTerGen::RQuadTreeTerrain::Border::SOUTHWEST) && (index == 0 || index == 1 || index == 4)) ++increment;
	if      (corner == (ProTerGen::RQuadTreeTerrain::Corner::NE) && (index == 8)) ++increment;
	else if (corner == (ProTerGen::RQuadTreeTerrain::Corner::NW) && (index == 6)) ++increment;
	else if (corner == (ProTerGen::RQuadTreeTerrain::Corner::SE) && (index == 2)) ++increment;
	else if (corner == (ProTerGen::RQuadTreeTerrain::Corner::SW) && (index == 0)) ++increment;
	return increment;
}


ProTerGen::Mesh ComputeChunksBasedOnFrontier(ProTerGen::RQuadTreeTerrain::Border frontier) noexcept
{
	using namespace ProTerGen;
	Mesh result{};
	float offset = 0.5f;
	for (uint32_t y = 0; y < NUM_VERTICES_PER_MINIMAL_PATCH_SIDE; ++y)
	{
		for (uint32_t x = 0; x < NUM_VERTICES_PER_MINIMAL_PATCH_SIDE; ++x)
		{
			if (x == 1 && y == 2 && RQuadTreeTerrain::ContainsNorth(frontier)) continue;
			if (x == 1 && y == 0 && RQuadTreeTerrain::ContainsSouth(frontier)) continue;
			if (x == 2 && y == 1 && RQuadTreeTerrain::ContainsEast(frontier)) continue;
			if (x == 0 && y == 1 && RQuadTreeTerrain::ContainsWest(frontier)) continue;
			Vertex v
			{
				.Position = { offset * x, 0.0f, offset * y, 0.0f},
				.Normal   = { 0.0f, 1.0f, 0.0f }, 
				.TexC     = { 0.0f, 1.0f }, 
				.TangentU = { 0.0f, 0.0f, -1.0f} 
			};
			result.Vertices.push_back(v);
		}
	}

	switch (frontier)
	{
	case RQuadTreeTerrain::Border::NONE:
		result.Indices = { 0,3,4,0,4,1,1,4,5,1,5,2,3,6,7,3,7,4,4,7,8,4,8,5 };
		break;
	case RQuadTreeTerrain::Border::NORTH:
		result.Indices = { 0,3,4,0,4,1,1,4,5,1,5,2,3,6,4,4,6,7,4,7,5 };
		break;
	case RQuadTreeTerrain::Border::NORTHEAST:
		result.Indices = { 0,3,4,0,4,1,1,4,2,2,4,6,3,5,4,4,5,6 };
		break;
	case RQuadTreeTerrain::Border::EAST:
		result.Indices = { 0,3,4,0,4,1,1,4,2,3,5,6,3,6,4,4,6,7,2,4,7 };
		break;
	case RQuadTreeTerrain::Border::SOUTHEAST:
		result.Indices = { 0,2,3,0,3,1,1,3,6,2,4,5,2,5,3,3,5,6 };
		break;
	case RQuadTreeTerrain::Border::SOUTH:
		result.Indices = { 0,2,3,0,3,1,1,3,4,2,5,6,2,6,3,3,6,7,3,7,4 };
		break;
	case RQuadTreeTerrain::Border::SOUTHWEST:
		result.Indices = { 0,2,1,1,2,3,0,4,2,2,4,5,2,5,6,2,6,3 };
		break;
	case RQuadTreeTerrain::Border::WEST:
		result.Indices = { 0,3,1,1,3,4,1,4,2,0,5,3,3,5,6,3,6,7,3,7,4};
		break;
	case RQuadTreeTerrain::Border::NORTHWEST:
		result.Indices = { 0,3,1,1,3,4,1,4,2,0,5,3,3,5,6,3,6,4 };
		break;
	}
	
	return result;
}

constexpr ProTerGen::Mesh GetMinimalQuad() noexcept
{
	using namespace ProTerGen;
	Mesh result{};
	result.Vertices =
	{
		Vertex {.Position = { 0.0f, 0.0f, 0.0f, 0.0f}, .Normal = { 0.0f, 1.0f, 0.0f }, .TexC = { 0.0f, 1.0f }, .TangentU = { 0.0f, 0.0f, 1.0f}},
		Vertex {.Position = { 1.0f, 0.0f, 0.0f, 0.0f}, .Normal = { 0.0f, 1.0f, 0.0f }, .TexC = { 1.0f, 1.0f }, .TangentU = { 0.0f, 0.0f, 1.0f}},
		Vertex {.Position = { 0.0f, 0.0f, 1.0f, 0.0f}, .Normal = { 0.0f, 1.0f, 0.0f }, .TexC = { 0.0f, 0.0f }, .TangentU = { 0.0f, 0.0f, 1.0f}},
		Vertex {.Position = { 1.0f, 0.0f, 1.0f, 0.0f}, .Normal = { 0.0f, 1.0f, 0.0f }, .TexC = { 1.0f, 0.0f }, .TangentU = { 0.0f, 0.0f, 1.0f}},
	};
	result.Indices =
	{
		0, 1, 2,
		1, 3, 2
	};
	
	return Mesh();
}

#pragma region TerrainChunksAsyncSystem

ProTerGen::TerrainChunksAsyncSystem::~TerrainChunksAsyncSystem()
{
	for (const ECS::Entity& entity : mEntities)
	{
		TerrainChunksAsyncComponent& tc = mRegister->GetComponent<TerrainChunksAsyncComponent>(entity);
		tc.Thread->Dispose();
	}
}

void ProTerGen::TerrainChunksAsyncSystem::Init()
{
	for (const ECS::Entity& entity : mEntities)
	{
		for (uint32_t i = 0; i < gNumFrames; ++i)
		{
			mMeshes.CreateNewMeshGpu(BuildUniqueId(entity, i));
		}
		TerrainChunksAsyncComponent& tc = mRegister->GetComponent<TerrainChunksAsyncComponent>(entity);
		for (const RQuadTreeTerrain::Border& b : RQuadTreeTerrain::GetBorders())
		{
			tc.Chunks[RQuadTreeTerrain::ToNumeral(b)] = ComputeChunksBasedOnFrontier(b);
		}
		tc.Loaded.OnRemove([&](Chunk c, TerrainChunksAsyncComponent::MeshIdx idx) { RemoveChunk(entity, c, idx); });
		tc.Loaded.Resize(MAX_CHUNKS);
		tc.Thread = std::make_unique<VT::PageThread<ChunkInfo>>();
		tc.Thread->MaxQueueSize((size_t)MAX_CHUNKS * 2);
		tc.Thread->OnRun([&] (ChunkInfo& ci) { return ProcessGeometryFromHeightData(ci); });
		tc.Thread->Init();
	}
}

void ProTerGen::TerrainChunksAsyncSystem::Update(double dt)
{
	for (const ECS::Entity& entity : mEntities)
	{
		TerrainChunksAsyncComponent& tc = mRegister->GetComponent<TerrainChunksAsyncComponent>(entity);

		std::unique_ptr<RQuadTreeTerrain> rootNode = nullptr;
		std::vector<RQuadTreeTerrain*> requests = ComputeQuadTree(mCamera.Position, mCamera.Frustum, rootNode, tc);
		std::sort(requests.begin(), requests.end(), [](RQuadTreeTerrain* a, RQuadTreeTerrain* b) { return a->GetDepth() > b->GetDepth(); });

		RequestMesh(requests, tc);
		tc.Thread->Update(nullptr, 1000000);
	}
}

void ProTerGen::TerrainChunksAsyncSystem::UpdateOnGpu(Microsoft::WRL::ComPtr<ID3D12Device> device, uint32_t currentFrame) 
{
	for (const ECS::Entity& entity : mEntities)
	{
		TerrainChunksAsyncComponent& tc = mRegister->GetComponent<TerrainChunksAsyncComponent>(entity);
		MeshGpu& mGpu = mMeshes.GetMeshGpu(BuildUniqueId(entity, currentFrame));
		MeshRendererComponent& mRC = mRegister->GetComponent<MeshRendererComponent>(entity);
		
		Mesh finalMesh = {};
		std::unique_lock lo(mMutex);
		for (auto& [c, it] : tc.Loaded.Items())
		{
			if (!tc.Requested.contains(c.GetHash())) continue;
			Mesh& chunk = *it;
			finalMesh.Indices.insert(finalMesh.Indices.end(), chunk.Indices.begin(), chunk.Indices.end());
			for (size_t i = 0; i < chunk.Indices.size(); ++i)
			{
				finalMesh.Indices[finalMesh.Indices.size() - 1 - i] += (uint32_t)finalMesh.Vertices.size();
			}
			finalMesh.Vertices.insert(finalMesh.Vertices.end(), chunk.Vertices.begin(), chunk.Vertices.end());
		}
		lo.unlock();

		if (finalMesh.Indices.size() == 0) finalMesh.Indices.push_back(0);
		if (finalMesh.Vertices.size() == 0) finalMesh.Vertices.push_back(Vertex{});
				
		CD3DX12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(finalMesh.Indices.size() * sizeof(Index));
		ThrowIfFailed(device->CreateCommittedResource
		(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(mGpu.IndexBufferGPU.ReleaseAndGetAddressOf())
		));
		mGpu.IndexBufferGPU->SetName((L"Terrain_" + std::to_wstring(entity) + L"_IndexBuffer").c_str());
		
		resDesc = CD3DX12_RESOURCE_DESC::Buffer(finalMesh.Vertices.size() * sizeof(Vertex));
		ThrowIfFailed(device->CreateCommittedResource
		(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(mGpu.VertexBufferGPU.ReleaseAndGetAddressOf())
		));
		mGpu.VertexBufferGPU->SetName((L"Terrain_" + std::to_wstring(entity) + L"_VertexBuffer").c_str());

		mGpu.IndexBufferByteSize = finalMesh.Indices.size() * sizeof(Index);
		mGpu.VertexBufferByteSize = finalMesh.Vertices.size() * sizeof(Vertex);
		mGpu.SubMesh[""].IndexCount = finalMesh.Indices.size();
		mGpu.VertexByteStride = sizeof(Vertex);

		void* indexPointer = nullptr;
		void* vertexPointer = nullptr;
		ThrowIfFailed(mGpu.IndexBufferGPU->Map(0, nullptr, &indexPointer));
		ThrowIfFailed(mGpu.VertexBufferGPU->Map(0, nullptr, &vertexPointer));
		
		memcpy(indexPointer, finalMesh.Indices.data(), finalMesh.Indices.size() * sizeof(Index));
		memcpy(vertexPointer, finalMesh.Vertices.data(), finalMesh.Vertices.size() * sizeof(Vertex));

		mGpu.IndexBufferGPU->Unmap(0, nullptr);
		mGpu.VertexBufferGPU->Unmap(0, nullptr);
		
		mRC.MeshGpuLocation = BuildUniqueId(entity, currentFrame);
	}
}

void ProTerGen::TerrainChunksAsyncSystem::OnEntityRemoved(ECS::Entity entity)
{
	TerrainChunksAsyncComponent& tc = mRegister->GetComponent<TerrainChunksAsyncComponent>(entity);
	tc.Thread->Dispose();
}


std::vector<ProTerGen::RQuadTreeTerrain*> ProTerGen::TerrainChunksAsyncSystem::ComputeQuadTree
(
	const DirectX::XMFLOAT3& camPos,
	const DirectX::BoundingFrustum& frustum,
	std::unique_ptr<RQuadTreeTerrain>& rootNode,
	const TerrainChunksAsyncComponent& tc
) const
{
	std::vector<RQuadTreeTerrain*> leafNodesResult;

	rootNode = std::make_unique<RQuadTreeTerrain>(RQuadTreeTerrain::Type::ROOT, nullptr, 0.0f, 0.0f, tc.TerrainSettings.TerrainWidth);
	rootNode->Subdivide(camPos, frustum, leafNodesResult, tc.TerrainSettings.TerrainWidth / (1 << tc.TerrainSettings.ChunksPerSideExp));
	rootNode->UpdateNeigbours();

	return leafNodesResult;
}

void ProTerGen::TerrainChunksAsyncSystem::RequestMesh(const std::vector<RQuadTreeTerrain*> request, TerrainChunksAsyncComponent& tc)
{
	const uint32_t chunkCount = 1 << tc.TerrainSettings.ChunksPerSideExp;
	const uint32_t maxLod = tc.TerrainSettings.ChunksPerSideExp; // FastLog2(chunkCount);
	const float halfSize = tc.TerrainSettings.TerrainWidth * 0.5f;

	tc.Requested.clear();
	for (const auto& qt : request)
	{
		const uint32_t lod = maxLod - FastLog2((uint32_t)(tc.TerrainSettings.TerrainWidth / (uint32_t)qt->EdgeLength()));
		const uint32_t ix = (uint32_t)(((qt->GetMinX() + halfSize) / tc.TerrainSettings.TerrainWidth) * chunkCount);
		const uint32_t iy = (uint32_t)(((qt->GetMinY() + halfSize) / tc.TerrainSettings.TerrainWidth) * chunkCount);
		Chunk c =
		{
			.x = (uint16_t)ix,
			.y = (uint16_t)iy,
			.lod = (uint8_t)lod,
			.border = (uint8_t)qt->GetBorder(),
			.corner = (uint8_t)0
		};
		TerrainChunksAsyncComponent::MeshIdx idx;
		std::unique_lock lo(mMutex);
		if (!tc.Loaded.TryGet(c, idx, true))
		{
			ChunkInfo ci(c, &tc);
			tc.Thread->Enqueue(ci);
		}
		lo.unlock();
		tc.Requested.insert(c.GetHash());
	}
}

bool ProTerGen::TerrainChunksAsyncSystem::ProcessGeometryFromHeightData(ChunkInfo& ci)
{
	const Chunk& c = ci.chunk;
	TerrainChunksAsyncComponent& tc = *ci.terrainComponent;
	const float halfSize = tc.TerrainSettings.TerrainWidth * 0.5f;
	const uint32_t chunkCount = 1 << tc.TerrainSettings.ChunksPerSideExp;
	const uint32_t maxLod = tc.TerrainSettings.ChunksPerSideExp; // FastLog2(chunkCount);
	const float invHalfSize = 1.0f / tc.TerrainSettings.TerrainWidth;
	const uint32_t edgeScale = (uint32_t)(tc.TerrainSettings.TerrainWidth / (1 << (maxLod - c.lod)));
	const float minScale = (float)edgeScale / maxLod;

	const float minX = ((float)c.x * tc.TerrainSettings.TerrainWidth / chunkCount) - halfSize;
	const float minY = ((float)c.y * tc.TerrainSettings.TerrainWidth / chunkCount) - halfSize;
	PerlinNoise n = PerlinNoise();
	n.Generate(1);
	Mesh m{};
	for (size_t y = 0; y < maxLod; ++y)
	{
		for (size_t x = 0; x < maxLod; ++x)
		{
			RQuadTreeTerrain::Border b = RQuadTreeTerrain::NONE;
			if (y == 0 && RQuadTreeTerrain::ContainsSouth((RQuadTreeTerrain::Border)c.border)) b = RQuadTreeTerrain::Border::SOUTH;
			if (y == maxLod - 1 && RQuadTreeTerrain::ContainsNorth((RQuadTreeTerrain::Border)c.border)) b = RQuadTreeTerrain::Border::NORTH;
			if (x == 0 && RQuadTreeTerrain::ContainsWest((RQuadTreeTerrain::Border)c.border)) b = RQuadTreeTerrain::Border::WEST;
			if (x == maxLod - 1 && RQuadTreeTerrain::ContainsEast((RQuadTreeTerrain::Border)c.border)) b = RQuadTreeTerrain::Border::EAST;
			if (y == 0 && x == 0 && c.border == (uint8_t)RQuadTreeTerrain::SOUTHWEST) b = RQuadTreeTerrain::Border::SOUTHWEST;
			if (y == 0 && x == maxLod - 1 && c.border == (uint8_t)RQuadTreeTerrain::SOUTHWEST) b = RQuadTreeTerrain::Border::SOUTHEAST;
			if (y == maxLod - 1 && x == 0 && c.border == (uint8_t)RQuadTreeTerrain::NORTHWEST) b = RQuadTreeTerrain::Border::NORTHWEST;
			if (y == maxLod - 1 && x == maxLod - 1 && c.border == (uint8_t)RQuadTreeTerrain::NORTHEAST) b = RQuadTreeTerrain::Border::NORTHEAST;
			Mesh d = tc.Chunks[RQuadTreeTerrain::ToNumeral(b)];
			for (size_t i = 0; i < d.Vertices.size(); ++i)
			{
				Vertex& v = d.Vertices[i];
				v.Position.x = v.Position.x * minScale + x * minScale + minX;
				v.Position.z = v.Position.z * minScale + y * minScale + minY;

				//v.Position.w = (float)c.lod + 0.01;
				v.Position.w = (float)c.lod + 0.01f;

				v.TexC.x = (halfSize + v.Position.x) * invHalfSize;
				v.TexC.y = (halfSize + v.Position.z) * invHalfSize;

				// TODO: process noise
				v.Position.y = (float)n.FBM(v.TexC.x, v.TexC.y, 6, 1.0f, 0.6f) * tc.TerrainSettings.Height - (tc.TerrainSettings.Height / 2);
				//v.Position.y = 15.0f * ((float)y * maxLod + x);
			}
			for (size_t i = 0; i < d.Indices.size(); ++i)
			{
				d.Indices[i] += (uint32_t)m.Vertices.size();
			}
			m.Vertices.insert(m.Vertices.end(), d.Vertices.begin(), d.Vertices.end());
			m.Indices.insert(m.Indices.end(), d.Indices.begin(), d.Indices.end());
		}
	}
	std::unique_lock lo(mMutex, std::defer_lock);
	if(&mMutex != nullptr && !lo.try_lock_for(std::chrono::milliseconds(1000))) return false;
	TerrainChunksAsyncComponent::MeshIdx idx;
	if (tc.Loaded.TryGet(c, idx, false)) return false;
	if (!tc.Offsets.empty())
	{
		idx = tc.Offsets.front();
		idx->Indices = std::move(m.Indices);
		idx->Vertices = std::move(m.Vertices);
		tc.Offsets.pop();
	}
	else
	{
		tc.MemoryChunks.push_back(std::move(m));
		idx = tc.MemoryChunks.end();
		--idx;
	}
	tc.Loaded.Add(c, idx);
	return true;
}

void ProTerGen::TerrainChunksAsyncSystem::RemoveChunk(ECS::Entity entity, Chunk& chunk, TerrainChunksAsyncComponent::MeshIdx index)
{
	TerrainChunksAsyncComponent& tc = mRegister->GetComponent<TerrainChunksAsyncComponent>(entity);
	tc.Offsets.emplace(index);
}

#pragma endregion

#pragma region TerrainQuadTreeSystem

void ProTerGen::TerrainQuadTreeSystem::Init()
{
	for (ECS::Entity entity : mEntities)
	{
		TerrainQTComponent& tc = mRegister->GetComponent<TerrainQTComponent>(entity);
		for (uint32_t i = 0; i < gNumFrames; ++i)
		{
			mMeshes.CreateNewMeshGpu(BuildUniqueId(entity, i));
		}
		for (const RQuadTreeTerrain::Border& b: RQuadTreeTerrain::GetBorders())
		{
			tc.Models[RQuadTreeTerrain::ToNumeral(b)] = ComputeChunksBasedOnFrontier(b);
		}
		
	}
}

void ProTerGen::TerrainQuadTreeSystem::Update(double dt)
{
	for (ECS::Entity entity : mEntities)
	{
		TerrainQTComponent& tc = mRegister->GetComponent<TerrainQTComponent>(entity);

		std::unique_ptr<RQuadTreeTerrain> rootNode = nullptr;
		std::vector<RQuadTreeTerrain*> requests = ComputeQuadTree(mCamera.Position, mCamera.Frustum, rootNode, tc);

		RequestMesh(requests, tc);

		if (tc.Mesh.Vertices.size() == 0)
		{
			tc.Mesh.Vertices = { {.Position = { 0.0f, 0.0f, 0.0f, 0.0f } } };
			tc.Mesh.Indices = { 0, 0, 0 };
		}
	}
}


void ProTerGen::TerrainQuadTreeSystem::UpdateOnGpu(Microsoft::WRL::ComPtr<ID3D12Device> device, uint32_t currentFrame)
{

	for (ECS::Entity entity : mEntities)
	{
		TerrainQTComponent& tc = mRegister->GetComponent<TerrainQTComponent>(entity);
		MeshRendererComponent& mrc = mRegister->GetComponent<MeshRendererComponent>(entity);

		const std::string id = BuildUniqueId(entity, currentFrame);
		MeshGpu& meshGpu = mMeshes.GetMeshGpu(id);

		meshGpu.IndexBufferByteSize = sizeof(Index) * static_cast<UINT64>(tc.Mesh.Indices.size());
		CD3DX12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(meshGpu.IndexBufferByteSize);
		ThrowIfFailed(device->CreateCommittedResource
		(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(meshGpu.IndexBufferGPU.ReleaseAndGetAddressOf())
		));
		char* pIndices = nullptr;
		ThrowIfFailed(meshGpu.IndexBufferGPU->Map(0, nullptr, reinterpret_cast<void**>(&pIndices)));
		memcpy(pIndices, tc.Mesh.Indices.data(), meshGpu.IndexBufferByteSize);
		meshGpu.IndexBufferGPU->Unmap(0, nullptr);
		pIndices = nullptr;

		meshGpu.SubMesh[""].IndexCount = tc.Mesh.Indices.size();
		tc.Mesh.Indices.clear();

		meshGpu.VertexBufferByteSize = sizeof(Vertex) * static_cast<UINT64>(tc.Mesh.Vertices.size());
		heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(meshGpu.VertexBufferByteSize);
		ThrowIfFailed(device->CreateCommittedResource
		(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(meshGpu.VertexBufferGPU.ReleaseAndGetAddressOf())
		));
		char* pVertices = nullptr;
		ThrowIfFailed(meshGpu.VertexBufferGPU->Map(0, nullptr, reinterpret_cast<void**>(&pVertices)));
		memcpy(pVertices, tc.Mesh.Vertices.data(), meshGpu.VertexBufferByteSize);
		meshGpu.VertexBufferGPU->Unmap(0, nullptr);
		pVertices = nullptr;

		meshGpu.VertexByteStride = sizeof(Vertex);
		tc.Mesh.Vertices.clear();

		mrc.MeshGpuLocation = id;
	}
}

void ProTerGen::TerrainQuadTreeSystem::OnEntityRemoved(ECS::Entity entity)
{
	for (uint32_t i = 0; i < gNumFrames; ++i)
	{
		mMeshes.RemoveMeshGpu(BuildUniqueId(entity, i));
	}
}

std::vector<ProTerGen::RQuadTreeTerrain*> ProTerGen::TerrainQuadTreeSystem::ComputeQuadTree
(
	const DirectX::XMFLOAT3& camPos,
	const DirectX::BoundingFrustum& frustum,
	std::unique_ptr<RQuadTreeTerrain>& rootNode,
	const TerrainQTComponent& tc
) const
{
	std::vector<RQuadTreeTerrain*> leafNodesResult;

	rootNode = std::make_unique<RQuadTreeTerrain>(RQuadTreeTerrain::Type::ROOT, nullptr, 0.0f, 0.0f, tc.TerrainSettings.TerrainWidth);
	rootNode->Subdivide(camPos, frustum, leafNodesResult, tc.TerrainSettings.TerrainWidth / (1 << tc.TerrainSettings.ChunksPerSideExp));
	rootNode->UpdateNeigbours();

	return leafNodesResult;
}

void ProTerGen::TerrainQuadTreeSystem::RequestMesh(const std::vector<RQuadTreeTerrain*> requests, TerrainQTComponent& tc)
{
	const float halfSize = tc.TerrainSettings.TerrainWidth * 0.5f;
	const uint32_t chunkCount = 1 << tc.TerrainSettings.ChunksPerSideExp;
	const uint32_t maxLod = tc.TerrainSettings.ChunksPerSideExp;//FastLog2(chunkCount);
	const float invHalfSize = 1.0f / tc.TerrainSettings.TerrainWidth;
	const float num = (float)tc.TerrainSettings.QuadsPerChunk;
	Mesh& m = tc.Mesh;
	for (size_t idx = 0; idx < requests.size(); ++idx)//(const auto& qt : leafNodes)
	{
		RQuadTreeTerrain& qt = *requests[idx];
		const uint32_t lod = (maxLod - qt.GetDepth());
		const float edgeScale = (tc.TerrainSettings.TerrainWidth / (1 << qt.GetDepth()));
		const float minScale = edgeScale / num;
		const RQuadTreeTerrain::Border border = qt.GetBorder();
		const RQuadTreeTerrain::Corner corner = qt.GetCorner();
		
		for (size_t y = 0; y < num; ++y)
		{
			for (size_t x = 0; x < num; ++x)
			{
				RQuadTreeTerrain::Border b = RQuadTreeTerrain::NONE;
				if      ( RQuadTreeTerrain::ContainsSouth(border) && y == 0      ) b = RQuadTreeTerrain::Border::SOUTH;
				else if ( RQuadTreeTerrain::ContainsNorth(border) && y == num - 1) b = RQuadTreeTerrain::Border::NORTH;
				if      ( RQuadTreeTerrain::ContainsWest (border) && x == 0      ) b = RQuadTreeTerrain::Border::WEST;
				else if ( RQuadTreeTerrain::ContainsEast (border) && x == num - 1) b = RQuadTreeTerrain::Border::EAST;
				if      ( border == RQuadTreeTerrain::SOUTHWEST && y == 0       && x == 0)       b = border;
				else if ( border == RQuadTreeTerrain::SOUTHEAST && y == 0       && x == num - 1) b = border;
				else if ( border == RQuadTreeTerrain::NORTHWEST && y == num - 1 && x == 0)       b = border;
				else if ( border == RQuadTreeTerrain::NORTHEAST && y == num - 1 && x == num - 1) b = border;
				RQuadTreeTerrain::Corner c = RQuadTreeTerrain::Corner::NONE;
				if      (corner == RQuadTreeTerrain::Corner::SW && y == 0       && x == 0      ) c = corner;
				else if (corner == RQuadTreeTerrain::Corner::SE && y == 0       && x == num - 1) c = corner;
				else if (corner == RQuadTreeTerrain::Corner::NW && y == num - 1 && x == 0      ) c = corner;
				else if (corner == RQuadTreeTerrain::Corner::NE && y == num - 1 && x == num - 1) c = corner;
				Mesh d = tc.Models[RQuadTreeTerrain::ToNumeral(b)];
				for (size_t i = 0; i < d.Vertices.size(); ++i)
				{
					Vertex& v = d.Vertices[i];
					v.Position.x = v.Position.x * minScale + x * minScale + qt.GetMinX();
					v.Position.z = v.Position.z * minScale + y * minScale + qt.GetMinY();
					v.Position.y = tc.TerrainSettings.Height;
					v.Position.w = (float)lod + 0.01f + ComputeMipIncrement((uint32_t)i, b, c);
					//v.Position.w = (float)RQuadTreeTerrain::ToNumeral(b) + 0.01f;
					v.TexC.x = (halfSize + v.Position.x) * invHalfSize;
					v.TexC.y = (halfSize + v.Position.z) * invHalfSize;
				}
				for (size_t i = 0; i < d.Indices.size(); ++i)
				{
					d.Indices[i] += (uint32_t)m.Vertices.size();
				}
				m.Vertices.insert(m.Vertices.end(), d.Vertices.begin(), d.Vertices.end());
				m.Indices.insert(m.Indices.end(), d.Indices.begin(), d.Indices.end());
			}
		}
	}
}

#pragma endregion

#pragma region TERRAIN_MORPH

double ProTerGen::TerrainQTMorphSystem::sMetricVertexCountAcc = 0.0;
size_t ProTerGen::TerrainQTMorphSystem::sMetricNumTimes       = 0;

void ProTerGen::TerrainQTMorphSystem::Init()
{
	for (ECS::Entity entity : mEntities)
	{
		for (uint32_t i = 0; i < gNumFrames; ++i)
		{
			mMeshes.CreateNewMeshGpu(BuildUniqueId(entity, i));
		}
		TerrainQTComponent& tc = mRegister->GetComponent<TerrainQTComponent>(entity);
		for (const RQuadTreeTerrain::Border& b: RQuadTreeTerrain::GetBorders())
		{
			tc.Models[RQuadTreeTerrain::ToNumeral(b)] = ComputeChunksBasedOnFrontier(b);
		}
		
		mIndexer.Init(mInfo.VTTilesPerRowExp);

		for (ECS::Entity particleSystem : tc.ParticleSystems)
		{
			DynamicParticleComponent& dgp = mRegister->GetComponent<DynamicParticleComponent>(particleSystem);
			const size_t totalParticles    = 1000000;
			const size_t numChunksSide     = ((size_t)1 << tc.TerrainSettings.ChunksPerSideExp);
			const float minWidth           = 1.0f / (float)numChunksSide;
			const float halfWidth          = 0.5f;
			const size_t particlesPerChunk = totalParticles / (numChunksSide * numChunksSide);
			dgp.MaxSize = totalParticles;
			dgp.ParticlesPerChunk = particlesPerChunk;
			dgp.PreloadedPositions.resize(totalParticles);
			for (size_t y = 0; y < numChunksSide; ++y)
			{
				for (size_t x = 0; x < numChunksSide; ++x)
				{
					const float offsetx = -halfWidth + (minWidth * x);
					const float offsety = -halfWidth + (minWidth * y);
					for (size_t i = 0; i < particlesPerChunk; ++i)
					{
						const float randx = offsetx + Map(0.0f, (float)RAND_MAX, 0.0f, minWidth, (float)std::rand());
						const float randy = offsety + Map(0.0f, (float)RAND_MAX, 0.0f, minWidth, (float)std::rand());
						dgp.PreloadedPositions[((x + numChunksSide * y) * particlesPerChunk) + i] = {randx, 0, randy};
					}
				}
			}
		}
	}
}

void ProTerGen::TerrainQTMorphSystem::Update(double dt)
{
	for (ECS::Entity entity : mEntities)
	{
		TerrainQTComponent& tc = mRegister->GetComponent<TerrainQTComponent>(entity);

		std::unique_ptr<RQuadTreeTerrain> rootNode = nullptr;
		std::vector<RQuadTreeTerrain*> requests = ComputeQuadTree(mCamera.Position, mCamera.Frustum, rootNode, tc);

		RequestMesh({mCamera.Position.x, mCamera.Position.z}, requests, tc);

		if (tc.Mesh.Vertices.size() == 0)
		{
			tc.Mesh.Vertices = { {.Position = { 0.0f, 0.0f, 0.0f, 0.0f } } };
			tc.Mesh.Indices = { 0, 0, 0 };
		}
	}
}

void ProTerGen::TerrainQTMorphSystem::UpdateOnGpu(Microsoft::WRL::ComPtr<ID3D12Device> device, uint32_t currentFrame)
{
	for (ECS::Entity entity : mEntities)
	{
		TerrainQTComponent& tc = mRegister->GetComponent<TerrainQTComponent>(entity);
		MeshRendererComponent& mrc = mRegister->GetComponent<MeshRendererComponent>(entity);

		if (mrc.NumTerrainMaterialLayers != tc.TerrainSettings.MaterialLayers.size())
		{
			mrc.NumTerrainMaterialLayers = (uint32_t)tc.TerrainSettings.MaterialLayers.size();
			mrc.NumFramesDirty           = gNumFrames;
		}

		const std::string id = BuildUniqueId(entity, currentFrame);
		MeshGpu& meshGpu = mMeshes.GetMeshGpu(id);

		meshGpu.IndexBufferByteSize = sizeof(Index) * static_cast<UINT64>(tc.Mesh.Indices.size());
		CD3DX12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(meshGpu.IndexBufferByteSize);
		ThrowIfFailed(device->CreateCommittedResource
		(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(meshGpu.IndexBufferGPU.ReleaseAndGetAddressOf())
		));
		char* pIndices = nullptr;
		ThrowIfFailed(meshGpu.IndexBufferGPU->Map(0, nullptr, reinterpret_cast<void**>(&pIndices)));
		memcpy(pIndices, tc.Mesh.Indices.data(), meshGpu.IndexBufferByteSize);
		meshGpu.IndexBufferGPU->Unmap(0, nullptr);
		pIndices = nullptr;

		meshGpu.SubMesh[""].IndexCount = tc.Mesh.Indices.size();
		tc.Mesh.Indices.clear();

		meshGpu.VertexBufferByteSize = sizeof(Vertex) * static_cast<UINT64>(tc.Mesh.Vertices.size());
		heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(meshGpu.VertexBufferByteSize);
		ThrowIfFailed(device->CreateCommittedResource
		(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(meshGpu.VertexBufferGPU.ReleaseAndGetAddressOf())
		));
		char* pVertices = nullptr;
		ThrowIfFailed(meshGpu.VertexBufferGPU->Map(0, nullptr, reinterpret_cast<void**>(&pVertices)));
		memcpy(pVertices, tc.Mesh.Vertices.data(), meshGpu.VertexBufferByteSize);
		meshGpu.VertexBufferGPU->Unmap(0, nullptr);
		pVertices = nullptr;

		meshGpu.VertexByteStride = sizeof(Vertex);
		tc.Mesh.Vertices.clear();

		if (mrc.MeshGpuLocation != id)
		{
			mrc.MeshGpuLocation = id;
			mrc.NumFramesDirty  = gNumFrames;
		}	
	}
}

void ProTerGen::TerrainQTMorphSystem::FillTerrainMaterialLayersStructuredBuffer(UploadBuffer<TerrainMaterialLayerConstants>* buffer)
{
	uint32_t ei = 0;
	for (const ECS::Entity& entity : mEntities)
	{
		const uint32_t         offset = ei * gMaxComputeLayers;
		const TerrainSettings& ts     = mRegister->GetComponent<TerrainQTComponent>(entity).TerrainSettings;
		for (uint32_t i = 0; i < ts.MaterialLayers.size(); ++i)
		{
			const MaterialLayer& ml = ts.MaterialLayers[i];
			TerrainMaterialLayerConstants tmlc
			{
				.MaterialIndex    = ml.material->MatCBIndex,
				.MinSlope         = ml.minSlope,
				.SlopeSmoothness  = ml.slopeSmoothness,
				.MinHeight        = ml.minHeight,
				.HeightSmoothness = ml.heightSmoothness,
			};
			buffer->CopyData(offset + (uint32_t)i, tmlc);
		}
		++ei;
	}
}

void ProTerGen::TerrainQTMorphSystem::OnEntityRemoved(ECS::Entity entity)
{
	for (uint32_t i = 0; i < gNumFrames; ++i)
	{
		mMeshes.RemoveMeshGpu(BuildUniqueId(entity, i));
	}
}

std::vector<ProTerGen::RQuadTreeTerrain*> ProTerGen::TerrainQTMorphSystem::ComputeQuadTree
(
	const DirectX::XMFLOAT3& camPos,
	const DirectX::BoundingFrustum& frustum,
	std::unique_ptr<RQuadTreeTerrain>& rootNode,
	const TerrainQTComponent& tc
) const
{
	std::vector<RQuadTreeTerrain*> leafNodesResult;

	rootNode = std::make_unique<RQuadTreeTerrain>(RQuadTreeTerrain::Type::ROOT, nullptr, 0.0f, 0.0f, tc.TerrainSettings.TerrainWidth);
	rootNode->Subdivide(camPos, frustum, leafNodesResult, tc.TerrainSettings.Height, tc.TerrainSettings.TerrainWidth / (1 << tc.TerrainSettings.ChunksPerSideExp));

	return leafNodesResult;
}

void ProTerGen::TerrainQTMorphSystem::RequestMesh(const DirectX::XMFLOAT2& camPos, const std::vector<RQuadTreeTerrain*> requests, TerrainQTComponent& tc)
{
	mRequests.clear();
	const float halfSize             = tc.TerrainSettings.TerrainWidth * 0.5f;
	const uint32_t chunkCount        = 1 << tc.TerrainSettings.ChunksPerSideExp;
	const uint32_t maxLod            = tc.TerrainSettings.ChunksPerSideExp;
	const float invTerrWidth         = 1.0f / tc.TerrainSettings.TerrainWidth;
	const float num                  = (float)tc.TerrainSettings.QuadsPerChunk;
	const RQuadTreeTerrain::Border b = RQuadTreeTerrain::Border::NONE;
	Mesh& m = tc.Mesh;
	for (size_t idx = 0; idx < requests.size(); ++idx)
	{
		RQuadTreeTerrain& qt = *requests[idx];
		const uint32_t lod        = (maxLod - qt.GetDepth());
		const float edgeScale     = (tc.TerrainSettings.TerrainWidth / (1 << qt.GetDepth()));
		const float minScale      = edgeScale / num;
		const float halfMinEdge   = halfSize / (float)chunkCount;
		const float halfMinRadius = SQRT2 * halfMinEdge;
		const float topEdge       = SQRT2 * NUMBER_PI * qt.EdgeLength();
		const float bottomEdge    = topEdge - qt.EdgeLength();
		const float range         = topEdge - bottomEdge;
		for (size_t y = 0; y < num; ++y)
		{
			for (size_t x = 0; x < num; ++x)
			{
				Mesh d = tc.Models[RQuadTreeTerrain::ToNumeral(b)];
				for (size_t i = 0; i < d.Vertices.size(); ++i)
				{
					Vertex& v = d.Vertices[i];
					v.Position.x = v.Position.x * minScale + x * minScale + qt.GetMinX();
					v.Position.z = v.Position.z * minScale + y * minScale + qt.GetMinY();

					const DirectX::XMFLOAT2 d = { camPos.x - v.Position.x, camPos.y - v.Position.z };
		            const float distance = sqrt((d.x * d.x) + (d.y * d.y));
					const float influence = ProTerGen_clamp(0.0f, 1.0f, (distance - bottomEdge) / range);
					v.Position.w = (float)(lod + influence);

					if (((i / 3) % 2) == 1)
					{
						v.Position.z += 0.5f * minScale * influence;
					}
					if (i % 3 == 1)
					{
						v.Position.x += 0.5f * minScale * influence;
					}
					v.Position.y = 1;
					if (v.Position.x >= halfSize - 0.1f || v.Position.x <= -halfSize + 0.1f
						|| v.Position.z >= halfSize - 0.1f || v.Position.z <= -halfSize + 0.1f)
					{
						v.Position.y = 0;
					}
					v.TexC.x = (halfSize + v.Position.x) * invTerrWidth;
					v.TexC.y = (halfSize + v.Position.z) * invTerrWidth;
				}
				for (size_t i = 0; i < d.Indices.size(); ++i)
				{
					d.Indices[i] += (uint32_t)m.Vertices.size();
				}
				m.Vertices.insert(m.Vertices.end(), d.Vertices.begin(), d.Vertices.end());
				m.Indices.insert(m.Indices.end(), d.Indices.begin(), d.Indices.end());
			}
		}

		if (lod < 2)
		{
			// TODO: change this to have the same particle pool for all (or 4 to choose) chunks
			const size_t chunksPerQuad = (size_t)1 << lod;
			const size_t offsetx = (size_t)((qt.GetMinX() + halfSize) * invTerrWidth * (float)chunkCount);
			const size_t offsety = (size_t)((qt.GetMinY() + halfSize) * invTerrWidth * (float)chunkCount);
			for (ECS::Entity entity : tc.ParticleSystems)
			{
				DynamicParticleComponent& dpc = mRegister->GetComponent<DynamicParticleComponent>(entity);
				const size_t offset = (offsetx * dpc.ParticlesPerChunk) + (offsety * dpc.ParticlesPerChunk * chunkCount);
				for (uint32_t y = 0; y < chunksPerQuad; ++y)
				{
					for (uint32_t x = 0; x < chunksPerQuad; ++x)
					{
						const size_t totOffset = offset + (x * dpc.ParticlesPerChunk) + (y * dpc.ParticlesPerChunk * chunkCount);
						const auto itBeg = dpc.PreloadedPositions.begin() + totOffset;
						const auto itEnd = itBeg + dpc.ParticlesPerChunk;
						dpc.ParticlePositions.insert(dpc.ParticlePositions.end(), itBeg, itEnd);
					}
				}
			}
		}

#ifdef QT_FEEDBACK_REQUESTS
		uint32_t iLod = lod;
		while (iLod <= mInfo.VTTilesPerRowExp)
		{
		    const uint32_t ix = (uint32_t)(((qt.GetMinX() + halfSize) * invTerrWidth) * (float)mInfo.VTTilesPerRow() / (1 << iLod));
		    const uint32_t iy = (uint32_t)(((qt.GetMinY() + halfSize) * invTerrWidth) * (float)mInfo.VTTilesPerRow() / (1 << iLod));
			const size_t index = mIndexer.PageIndex(VT::Page{ .X = ix, .Y = iy, .Mip = iLod });
			mRequests[index] += iLod + 1;
			iLod += 1;
		}
#endif
	}
}

double ProTerGen::TerrainQTMorphSystem::MetricGetVertexCountMean()
{
	return sMetricVertexCountAcc / sMetricNumTimes;
}

void ProTerGen::TerrainQTMorphSystem::MetricResetCountMean()
{
	sMetricVertexCountAcc = 0.0f;
	sMetricNumTimes       = 0;
}

#pragma endregion

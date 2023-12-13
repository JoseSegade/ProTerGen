#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include "CommonHeaders.h"
#include "Mesh.h"

namespace ProTerGen
{
	struct MeshGpuLocations
	{
		D3D12_VERTEX_BUFFER_VIEW Vbv{};
		D3D12_INDEX_BUFFER_VIEW Ibv{};
		D3D12_VERTEX_BUFFER_VIEW Instances{};
		SubmeshParameters MeshRenderValues{};
	};

	class Meshes
	{
	public:
		Meshes() = default;
		virtual ~Meshes() = default;

		Mesh* LoadMeshFromFile(std::wstring path);
		MeshGpu& CreateNewMeshGpu(std::string name);
		void RemoveMeshGpu(std::string name);
		MeshGpu& GetMeshGpu(std::string name);

		void LoadMeshesInGpu
		(
			Microsoft::WRL::ComPtr<ID3D12Device> device,
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
			const std::vector<Mesh*>& meshes,
			const std::string name
		);

		MeshGpuLocations GetMeshGpuLocation(std::string meshName, std::string submeshName = "");

	protected:
		std::unordered_map<std::string, MeshGpu> mMeshes = {};
	};
}

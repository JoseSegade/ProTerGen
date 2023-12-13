#pragma once

#include "CommonHeaders.h"
#include "ECS.h"
#include "QuadTree.h"
#include "Meshes.h"
#include "Mesh.h"
#include "CameraSystem.h"

namespace ProTerGen
{
	struct TerrainQTQuadComponent
	{
		float    side             = 0.0f;
		uint32_t chunksPerSideExp = 0;
		float    height           = 0.0f;
		uint32_t numGpuDivisions  = 0;
		uint32_t numCpuDivisions  = 0;
	};

	class TerrainQTQuadSystem : public ECS::ECSSystem<TerrainQTQuadComponent>
	{
	public:
		explicit TerrainQTQuadSystem(CameraComponent* camera, Meshes* meshes) : mMeshes(meshes), mCamera(mCamera) {};
		virtual ~TerrainQTQuadSystem() {};

		inline void ChangeCamera(CameraComponent* newCam) { mCamera = newCam; }
		inline void ChangeMeshes(Meshes* newMeshes) { mMeshes = newMeshes; }

		void Init() override;
		void Update(double dt) override;

		void UpdateOnGpu(Microsoft::WRL::ComPtr<ID3D12Device> device, uint32_t currentFrame);

	private:
        using Index    = uint32_t;
        using Indices  = std::vector<Index>;
        using Vertices = std::vector<Vertex>;

		Meshes*          mMeshes = nullptr;
		CameraComponent* mCamera = nullptr;
	};
}

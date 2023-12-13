#pragma once

#include <DirectXMath.h>
#include "ECS.h"
#include "CommonHeaders.h"
#include "FrameResources.h"
#include "Mesh.h"
#include "Config.h"
#include "Material.h"
#include "Meshes.h"

namespace ProTerGen
{
	struct MeshRendererComponent
	{
		DirectX::XMFLOAT4X4 World =
		{
		   1.0f, 0.0f, 0.0f, 0.0f,
		   0.0f, 1.0f, 0.0f, 0.0f,
		   0.0f, 0.0f, 1.0f, 0.0f,
		   0.0f, 0.0f, 0.0f, 1.0f
		};

		DirectX::XMFLOAT4X4 TexTransform =
		{
		   1.0f, 0.0f, 0.0f, 0.0f,
		   0.0f, 1.0f, 0.0f, 0.0f,
		   0.0f, 0.0f, 1.0f, 0.0f,
		   0.0f, 0.0f, 0.0f, 1.0f
		};

		std::string MeshGpuLocation          = "";
		std::string SubmeshLocation          = "";
		Material*   Mat                      = nullptr;
		size_t      Layer                    = 0;
		uint32_t    NumTerrainMaterialLayers = 0;
		uint32_t    ObjCBIndex               = 0xffffffff;
		uint32_t    NumFramesDirty           = gNumFrames;
	};

	struct RenderContext
	{
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> CommandList = nullptr;
		size_t Layer = 0;
		D3D12_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	};

	class RenderSystem : public ECS::ECSSystem<MeshRendererComponent>
	{
	public:
		RenderSystem(FrameResources** frameResource, size_t layerCount, Meshes& meshes);
		virtual ~RenderSystem() = default;

		void Init() override;
		void Update(double deltaTime) override;
		void Render(const RenderContext& context);
		void Instance(const RenderContext& context);

	protected:
		FrameResources** mFrame;
		Meshes& mMeshes;
		std::vector<std::set<ECS::Entity>> mLayers;
		size_t mLayerCount = 1;
	};
}
#pragma once

#include <unordered_map>

#include <array>
#include "CommonHeaders.h"
#include "Config.h"
#include "App.h"
#include "ECS.h"
#include "FrameResources.h"
#include "Texture.h"
#include "Menus.h"
#include "TerrainTree.h"
#include "Materials.h"
#include "Shaders.h"
#include "Meshes.h"
#include "CameraSystem.h"
#include "PerspectiveVisualizationSystem.h"
#include "Mesh.h"
#include "VirtualTexture.h"
#include "TileGenerator.h"
#include "GpuBatches.h"
#include "CustomHeightmapTileGenerator.h"
#include "PageLoaderGpuGen.h"
#include "GrassCompute.h"
#include "CascadeShadowMap.h"

namespace ProTerGen
{
	enum class RenderLayer : size_t
	{
		Opaque = 0,
		AlphaClip,
		OpaqueTerrain,
		Debug,
		Sky,
		VT_Debug,
		GrassParticles,
		InstanceBasic,
		OpaqueTerrainQuad,
		Count,
	};

	class MainEngine : public ProTerGen::App
	{
	public:
		MainEngine();
		~MainEngine();

		virtual bool Init(HWND winHandle, uint32_t width, uint32_t height)override;

	private:
		virtual void CreateDescriptorHeaps() override;
		virtual void OnResize() override;
		virtual void Update(const Clock& gt) override;
		virtual void Draw(const Clock& gt) override;

		void UpdateImgui(const Clock& gt);
		void UpdateObjectCBs(const Clock& gt);
		void UpdateStructuredBuffers(const Clock& gt);
		void UpdateConstantBuffers(const Clock& gt);

		void CreateEntites();
		void CreateCameras();
		void CreateRenderizables();

		void InitImgui(HWND winHandle);
		void LoadTextures();
		void GenerateVirtualCache();
		void BuildCSM();
		void BuildRootSignature();
		void BuildShadersAndInputLayout();
		void BuildGeometry();
		void BuildPSOs();
		void BuildFrameResources();
		void BuildMaterials();
		void BuildEnvironmentSettings();
		void BuildMenus();

		void DrawRenderItems(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, RenderLayer renderLayer, D3D12_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		void InstanceRenderItems(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, RenderLayer renderLayer, D3D12_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		void ExecuteCommandList();
		void OpenCommandList();

	private:

		struct FrameResource
		{
			std::vector<std::unique_ptr<FrameResources>> FrameResources{};
			ProTerGen::FrameResources*                   CurrFrameResource      = nullptr;
			uint32_t                                     CurrFrameResourceIndex = 0;
		};

		FrameResource mFrameResource;

		GpuBatches mBatches;

		Materials mMaterials{};
		Shaders   mShaders{};
		Meshes    mMeshes{};

		ECS::Register mRegister = {};

		PassConstants mMainPassCB = {};
		ECS::Entity   mMainConfig = ECS::INVALID;

		ECS::Entity mTerrain    = ECS::INVALID;
		ECS::Entity mTerrainAlt = ECS::INVALID;

		VT::VTDesc mVTTerrainDesc = {};
		VT::VirtualTexture<VT::PageGpuGen_Sdh> mVTTerrain = {};
		VT::FeedbackBuffer mFeedbackBuffer = {};

		CascadeShadowMap mCSM = {};

		//VT::CustomHeightmapTileGenerator hGen = {};

		GrassCompute mGrass = {};
	};
}
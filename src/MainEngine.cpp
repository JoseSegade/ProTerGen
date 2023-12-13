#include <DirectXMath.h>
#include <DirectXCollision.h>
#include "MainEngine.h"
#include "BufferCreator.h"
#include "Samplers.h"
#include "RenderSystem.h"
#include "MeshGenerator.h"
#include "PageLoaderGpuGen.h"
#include "ParticleSystem.h"
#include "TerrainQuad.h"
#if _DEBUG && PRINT_PERFORMANCE_TIMES
#include "Timer.h"
#endif

ProTerGen::MainEngine::MainEngine()
	: App()
{
}

ProTerGen::MainEngine::~MainEngine()
{
	if (mDevice)
	{
		FlushCommandQueue();
	}

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

bool ProTerGen::MainEngine::Init(HWND winHandle, uint32_t width, uint32_t height)
{
	if (!App::Init(winHandle, width, height))
	{
		return false;
	}
	ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

	
	LoadTextures();
	GenerateVirtualCache();
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildGeometry();
	BuildMaterials();
	CreateEntites();
	BuildFrameResources();
	BuildPSOs();
	BuildEnvironmentSettings();
	BuildCSM();

	//GrassComputeSettings gcs = { .radius = 10.0f, .maxNumber = 1024 * 1024 };
	//mGrass.Init(mDevice, mCommandList, mShaders, mDescriptorHeaps, gcs);

	BuildMenus();
	InitImgui(winHandle);

	mRegister.Init();

	ExecuteCommandList();
	FlushCommandQueue();

	return true;
}

void ProTerGen::MainEngine::CreateDescriptorHeaps()
{
	const uint32_t rtvCount = gNumFrames + gFeedbackBufferRTs; 
	const uint32_t dsvCount = gNumFrameDSs + gFeedbackBufferDSs + gNumCascadeShadowMaps;
	const uint32_t srvCount = gNumSRV;   
	const uint32_t samCount = 0;
	mDescriptorHeaps = DescriptorHeaps(mDevice, rtvCount, dsvCount, srvCount, samCount);
	mDescriptorHeaps.BuildSRVTable
	({
		{ "INDIRECTION_TEXTURES", gNumIndirectionTextures },
		{ "ATLAS_TEXTURES",       gNumAtlasTextures },
		{ "TEXTURES",             gNumTextures },
		{ "TERRAIN_TEXTURES",     gNumTerrainTextures },
		{ "TEXTURES_IMGUI",       gNumTexturesImGui },
		{ "CASCADE_SHADOW_MAPS",  gNumCascadeShadowMaps },
		{ "COMPUTE_TEXTURES",     gNumComputeTextures },
	});
}

void ProTerGen::MainEngine::LoadTextures()
{
	{
		// Default texture
		const std::vector<std::string> texNames
		{
			mMaterials.GetDefaultTextureColorName(),
			mMaterials.GetDefaultTextureNormalName(),
		};
		mMaterials.CreateDefaultTextures(mDevice, mCommandList);
		mMaterials.InsertTexturesInSRVHeap(mDevice, mDescriptorHeaps, "TEXTURES", texNames);
		mMaterials.InsertTexturesInSRVHeap(mDevice, mDescriptorHeaps, "TERRAIN_TEXTURES", texNames);
	}
	{
		// Terrain textures
		const std::vector<std::string> texNames
		{
		   "sandColorMap",
		   "grassColorMap",
		   "stoneColorMap",
		   "iceColorMap"
		};
		const std::vector<std::wstring> texFilenames
		{
			gTexturesPath + L"sand2.dds",
			gTexturesPath + L"grass_short.dds",
			gTexturesPath + L"stone1.dds",
			gTexturesPath + L"ice_seamless.dds",
		};
		for (size_t i = 0; i < texNames.size(); ++i)
		{
			mMaterials.CreateTexture(mDevice, mCommandList, texNames[i], texFilenames[i], true);
		}
		mMaterials.InsertTexturesInSRVHeap(mDevice, mDescriptorHeaps, "TERRAIN_TEXTURES", texNames);
	}
	{
		// Textures
		const std::vector<std::string> texNames
		{
	   		"grassBillboardColorMap",
	   		"barkColorMap",
	   		"barkNormalMap",
	   		"leavesColorMap",
	   		"leavesNormalMap",
		};
		const std::vector<std::wstring> texFilenames
		{
			gTexturesPath + L"grass_billboard.png",
			gTexturesPath + L"bark_albedo.png",
			gTexturesPath + L"bark_normal.png",
			gTexturesPath + L"leaves_albedo.png",
			gTexturesPath + L"leaves_normal.png",
		};
		for (size_t i = 0; i < texNames.size(); ++i)
		{
			mMaterials.CreateTexture(mDevice, mCommandList, texNames[i], texFilenames[i], true);
		}
		mMaterials.InsertTexturesInSRVHeap(mDevice, mDescriptorHeaps, "TEXTURES", texNames);
	}
}

void ProTerGen::MainEngine::GenerateVirtualCache()
{
	const size_t FEEDBACK_TEXTURE_SIZE  = 64;
	const uint32_t VT_UPLOADS_PER_FRAME = 5;
	
	mBatches.Init(mDevice);
	const auto& executePipeline = [&](GpuComputePipeline& computePipeline)
	{
		mBatches.ExecuteGpuPipeline(computePipeline);
		mBatches.WaitForGpuCompletion();
	};	
	const uint32_t batchId = mBatches.CreateGpuComputePipeline(mDevice);
	std::unique_ptr<VT::ComputeContext> ccPtr(new VT::ComputeContext
	{
		.device              = mDevice,
		.pipeline            = mBatches.GetGpuComputePipeline(batchId),
		.shaders             = mShaders,
		.materials           = mMaterials,
		.descriptorHeaps     = mDescriptorHeaps,
		.dispatchExecuteArgs = executePipeline
	});

	mVTTerrainDesc =
	{
		.VTSize           = 32 * 1024,
		.VTTilesPerRowExp = 7,
		.AtlasTilesPerRow = 32, // (6)8, (7)16, (8)32, ...
		.BorderSize       = 8,
	};

	const std::vector<std::string> indirectionTextureNames = { "VT_Indirection" };
	const std::vector<std::string> atlasTextureNames       = { "VT_AtlasHeightmap" };

	std::array<Texture*, decltype(mVTTerrain)::TEXTURES_COUNT()> atlasTextures =
	{
		mMaterials.CreateEmptyTexture(atlasTextureNames[0], Materials::TextureType::TEXTURE_SRV),
	};
	Texture* texInd = mMaterials.CreateEmptyTexture(indirectionTextureNames[0], Materials::TextureType::TEXTURE_SRV);
	mFeedbackBuffer.Init(mDevice, mDescriptorHeaps, &mVTTerrainDesc, FEEDBACK_TEXTURE_SIZE);
	mFeedbackBuffer.SetAsReadable(mCommandList);
	mVTTerrain.Init(mDevice, atlasTextures, texInd, &mVTTerrainDesc, VT_UPLOADS_PER_FRAME, ccPtr);

	mMaterials.InsertTexturesInSRVHeap(mDevice, mDescriptorHeaps, "INDIRECTION_TEXTURES", indirectionTextureNames);
	mMaterials.InsertTexturesInSRVHeap(mDevice, mDescriptorHeaps, "ATLAS_TEXTURES",       atlasTextureNames);
}

void ProTerGen::MainEngine::BuildRootSignature()
{
	const uint32_t VIRTUAL_TEXTURES_SLOT  = 1;
	const uint32_t STRUCTURED_BUFFER_SLOT = 2;
	const uint32_t DEFAULT_TEXTURES_SLOT  = 3;

	const std::string rootSignatureName = "DefaultRS";
	const std::string rootParamNames[] =
	{
		"Obj_CB", 
		"Pass_CB",
		"VT_CB",
		"Terrain_CB",
		"Shadow_CB", 
		"PerShadow_CB",

		"Mat", 
		"Shadow",
		"TerMatLayer",

		"Indirection_Tex",
		"Atlas_Tex",

		"Table_Tex",
		"Table_Shadow_Depth_Tex",
	};

	size_t i = 0;
	mShaders.AddConstantBufferToRootSignature(rootParamNames[i++], rootSignatureName);
	mShaders.AddConstantBufferToRootSignature(rootParamNames[i++], rootSignatureName);
	mShaders.AddConstantBufferToRootSignature(rootParamNames[i++], rootSignatureName);
	mShaders.AddConstantBufferToRootSignature(rootParamNames[i++], rootSignatureName);
	mShaders.AddConstantBufferToRootSignature(rootParamNames[i++], rootSignatureName);
	mShaders.AddConstantBufferToRootSignature(rootParamNames[i++], rootSignatureName);

	mShaders.AddShaderResourceViewToRootSignature(rootParamNames[i++], rootSignatureName, STRUCTURED_BUFFER_SLOT, D3D12_SHADER_VISIBILITY_ALL);
	mShaders.AddShaderResourceViewToRootSignature(rootParamNames[i++], rootSignatureName, STRUCTURED_BUFFER_SLOT, D3D12_SHADER_VISIBILITY_ALL);
	mShaders.AddShaderResourceViewToRootSignature(rootParamNames[i++], rootSignatureName, STRUCTURED_BUFFER_SLOT, D3D12_SHADER_VISIBILITY_ALL);
	
	mShaders.AddTableToRootSignature
	(
		rootParamNames[i++], 
		rootSignatureName, 
		1, 
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 
		D3D12_SHADER_VISIBILITY_ALL, 
		VIRTUAL_TEXTURES_SLOT
	);
	mShaders.AddTableToRootSignature
	(
		rootParamNames[i++], 
		rootSignatureName, 
		1, 
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 
		D3D12_SHADER_VISIBILITY_ALL, 
		VIRTUAL_TEXTURES_SLOT
	);

	mShaders.AddTableToRootSignature
	(
		rootParamNames[i++], 
		rootSignatureName, 
		gNumTextures, 
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV
	);
	mShaders.AddTableToRootSignature
	(
		rootParamNames[i++], 
		rootSignatureName, 
		gNumCascadeShadowMaps, 
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV
	);

	mShaders.SetSamplersToRootSignature(rootSignatureName, GetStaticSamplers());

	mShaders.CompileRootSignature(mDevice, rootSignatureName);
}

void ProTerGen::MainEngine::BuildShadersAndInputLayout()
{
	mShaders.InputLayout("DefaultInputLayout") =
	{
	   { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	   { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	   { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	   { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
	mShaders.InputLayout("PositionOnlyInputLayout") =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
	mShaders.InputLayout("InstanceInputLayout") =
	{
	   { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	   { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	   { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	   { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

		{ "INSTANCE_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
	};
	char numCascades[2];
	_itoa_s(gNumCascadeShadowMaps, numCascades, 10);
	{
		const std::vector<D3D_SHADER_MACRO> defines
		{
			D3D_SHADER_MACRO{ "NUM_SHADOW_SPLIT", numCascades },
			D3D_SHADER_MACRO{ NULL, NULL }
		};
		const std::vector<std::string> names =
		{
			"defaultVS",
			"defaultPS",
		    "terrainVS",
		    "terrainHS",
		    "terrainDS",
		    "terrainPS",
		    "grassParticlesVS",
		    "grassParticlesGS",
		    "grassParticlesPS",
		    "skyVS",
		    "skyPS",
		    "vtVS",
		    "vtPS",
		    "vtdebugVS",
		    "vtdebugPS",
			"shadowCasterVS",
			"shadowCasterPS",
			"debugVS",
			"debugPS",
			"terrainQuadVS",
			"terrainQuadHS",
			"terrainQuadDS",
			"terrainQuadPS"
		};
		const std::vector<std::wstring> paths =
		{
			gShadersPath + L"DefaultBlinnPhong.hlsl",
			gShadersPath + L"DefaultBlinnPhong.hlsl",
		    gShadersPath + L"TerrainTriMorph.hlsl",
		    gShadersPath + L"TerrainTriMorph.hlsl",
		    gShadersPath + L"TerrainTriMorph.hlsl",
		    gShadersPath + L"TerrainTriMorph.hlsl",
		    gShadersPath + L"GrassParticles.hlsl",
		    gShadersPath + L"GrassParticles.hlsl",
		    gShadersPath + L"GrassParticles.hlsl",
		    gShadersPath + L"Sky.hlsl",
		    gShadersPath + L"Sky.hlsl",
		    gShadersPath + L"VTFeedback.hlsl",
		    gShadersPath + L"VTFeedback.hlsl",
		    gShadersPath + L"VT_Debug.hlsl",
		    gShadersPath + L"VT_Debug.hlsl",
			gShadersPath + L"ShadowCaster.hlsl",
			gShadersPath + L"ShadowCaster.hlsl",
			gShadersPath + L"Debug.hlsl",
			gShadersPath + L"Debug.hlsl",
			gShadersPath + L"TerrainByQuad.hlsl",
			gShadersPath + L"TerrainByQuad.hlsl",
			gShadersPath + L"TerrainByQuad.hlsl",
			gShadersPath + L"TerrainByQuad.hlsl",
		};
		const std::vector<Shaders::TYPE> types =
		{
		    Shaders::TYPE::VS,
		    Shaders::TYPE::PS,
		    Shaders::TYPE::VS,
		    Shaders::TYPE::HS,
		    Shaders::TYPE::DS,
		    Shaders::TYPE::PS,
		    Shaders::TYPE::VS,
		    Shaders::TYPE::GS,
		    Shaders::TYPE::PS,
		    Shaders::TYPE::VS,
		    Shaders::TYPE::PS,
		    Shaders::TYPE::VS,
		    Shaders::TYPE::PS,
		    Shaders::TYPE::VS,
		    Shaders::TYPE::PS,
		    Shaders::TYPE::VS,
		    Shaders::TYPE::PS,
		    Shaders::TYPE::VS,
		    Shaders::TYPE::PS,
		    Shaders::TYPE::VS,
		    Shaders::TYPE::HS,
		    Shaders::TYPE::DS,
		    Shaders::TYPE::PS,
		};
		mShaders.CompileShaders(names, paths, types, defines);
	}
	{
		const std::vector<D3D_SHADER_MACRO> defines
		{
			D3D_SHADER_MACRO{ "NUM_SHADOW_SPLIT", numCascades },
			D3D_SHADER_MACRO{ "ALPHA_CLIP", "" },
			D3D_SHADER_MACRO{ NULL, NULL }
		};
		const std::vector<std::string> names
		{
			"alphaClipPS",
		};
		const std::vector<std::wstring> paths =
		{
		   gShadersPath + L"DefaultBlinnPhong.hlsl",
		};
		const std::vector<Shaders::TYPE> types =
		{
		   Shaders::TYPE::PS,
		};
		mShaders.CompileShaders(names, paths, types, defines);
	}
	{
		const std::vector<D3D_SHADER_MACRO> defines
		{
			D3D_SHADER_MACRO{ "NUM_SHADOW_SPLIT", numCascades },
			D3D_SHADER_MACRO{ "DEBUG_LOD_LEVEL", "" },
			D3D_SHADER_MACRO{ NULL, NULL }
		};
		const std::vector<std::string> names
		{
			"terrainDebugLodPS",
		};
		const std::vector<std::wstring> paths =
		{
		   gShadersPath + L"TerrainTriMorph.hlsl",
		};
		const std::vector<Shaders::TYPE> types =
		{
		   Shaders::TYPE::PS,
		};
		mShaders.CompileShaders(names, paths, types, defines);
	}
	{
		const std::vector<D3D_SHADER_MACRO> defines
		{
			D3D_SHADER_MACRO{ "NUM_SHADOW_SPLIT", numCascades },
			D3D_SHADER_MACRO{ NULL, NULL }
		};
		const std::vector<std::string> names
		{
			"instanceBasicVS",
			"instanceBasicPS",
		};
		const std::vector<std::wstring> paths =
		{
			gShadersPath + L"InstanceBasic.hlsl",
			gShadersPath + L"InstanceBasic.hlsl",
		};
		const std::vector<Shaders::TYPE> types =
		{
			Shaders::TYPE::VS,
			Shaders::TYPE::PS,
		};
		mShaders.CompileShaders(names, paths, types, defines);
	}
	{
		const std::vector<D3D_SHADER_MACRO> defines
		{
			D3D_SHADER_MACRO{ "NUM_SHADOW_SPLIT", numCascades },
			D3D_SHADER_MACRO{ "TERRAIN_SHADOW_CASTER", "" },
			D3D_SHADER_MACRO{ NULL, NULL }
		};
		const std::vector<std::string> names
		{
			"shadowCasterTerrainDS"
		};
		const std::vector<std::wstring> paths
		{
			gShadersPath + L"ShadowCaster.hlsl"
		};
		const std::vector<Shaders::TYPE> types
		{
			Shaders::TYPE::DS
		};
		mShaders.CompileShaders(names, paths, types, defines);
	}
}

void ProTerGen::MainEngine::BuildGeometry()
{
	MeshGenerator meshGen{};

	{
		Mesh skyDome = meshGen.CreateCube("sky_dome");
		std::vector<Mesh*> meshes;
		meshes.push_back(&skyDome);
		mMeshes.LoadMeshesInGpu(mDevice, mCommandList, meshes, "sky_mesh");
	}
	{
		Mesh quad = meshGen.CreateQuad("quad", 2, 2.0f, 2.0f);
		std::vector<Mesh*> meshes;
		meshes.push_back(&quad);
		mMeshes.LoadMeshesInGpu(mDevice, mCommandList, meshes, "vtdebug_quad");
	}
	{
		Mesh instanceCubes = meshGen.CreateCube("instance_cubes");
		std::vector<Mesh*> meshes;
		meshes.push_back(&instanceCubes);
		mMeshes.LoadMeshesInGpu(mDevice, mCommandList, meshes, "instance_cubes_mesh");
		MeshGpu& meshGpu = mMeshes.GetMeshGpu("instance_cubes_mesh");

		D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		const D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(DirectX::XMFLOAT3) * 6);
		ThrowIfFailed(mDevice->CreateCommittedResource
		(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(meshGpu.InstanceBufferGPU.ReleaseAndGetAddressOf())
		));
		meshGpu.InstanceBufferGPU->SetName(L"Debug_Instance_Cubes");
		heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		ThrowIfFailed(mDevice->CreateCommittedResource
		(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(meshGpu.InstanceBufferUploader.ReleaseAndGetAddressOf())
		));
		meshGpu.InstanceBufferUploader->SetName(L"Debug_Instance_Cubes_Upload");
		meshGpu.InstanceBufferByteSize = sizeof(DirectX::XMFLOAT3) * 6;
		meshGpu.InstanceBufferStride = sizeof(DirectX::XMFLOAT3);
		meshGpu.SubMesh["instance_cubes"].NumInstances = 6;

		const float scale = 50.0f;
		const DirectX::XMFLOAT3 positions[6] =
		{
			{  0.0f, 80.0f,   0.0f},
			{  0.0f, 80.0f,  scale},
			{ scale, 80.0f,  scale},
			{ scale, 80.0f,   0.0f},
			{-scale, 80.0f,   0.0f},
			{-scale, 80.0f, -scale},
		};
		void* mappedPtr = nullptr;
		ThrowIfFailed(meshGpu.InstanceBufferUploader->Map(0, nullptr, &mappedPtr));
		memcpy(mappedPtr, positions, sizeof(DirectX::XMFLOAT3) * 6);
		meshGpu.InstanceBufferUploader->Unmap(0, nullptr);
		mappedPtr = nullptr;
		
		CD3DX12_RESOURCE_BARRIER barriers[1] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(meshGpu.InstanceBufferGPU.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST),
		};
		mCommandList->ResourceBarrier(_countof(barriers), barriers);
		mCommandList->CopyBufferRegion(meshGpu.InstanceBufferGPU.Get(), 0, meshGpu.InstanceBufferUploader.Get(), 0, sizeof(DirectX::XMFLOAT3) * 6);
		barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(meshGpu.InstanceBufferGPU.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		mCommandList->ResourceBarrier(_countof(barriers), barriers);
	}
	{
		Mesh* objTreeLog    = mMeshes.LoadMeshFromFile((gObjsPath + L"simple_tree_log.obj").c_str());
		Mesh* objTreeLeaves = mMeshes.LoadMeshFromFile((gObjsPath + L"simple_tree_leaves.obj").c_str());
		objTreeLog->Name    = "simple_tree_log";
		objTreeLeaves->Name = "simple_tree_leaves";
		std::vector<Mesh*> meshes;
		meshes.push_back(objTreeLog);
		meshes.push_back(objTreeLeaves);
		mMeshes.LoadMeshesInGpu(mDevice, mCommandList, meshes, "simple_tree");
	}
}

void ProTerGen::MainEngine::BuildMaterials()
{
	{
		Material* mat      = mMaterials.CreateMaterial(mMaterials.GetDefaultMaterialName());
		mat->DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		mat->Diffuse       = mMaterials.GetDefaultTextureColorName();
		mat->Normal        = mMaterials.GetDefaultTextureNormalName();
	}
	{
		Material* ground = mMaterials.CreateMaterial("TERRAIN_0");
		ground->DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		ground->Diffuse       = "sandColorMap";
		ground->Normal        = mMaterials.GetDefaultTextureNormalName();
	}
	{
		Material* ground = mMaterials.CreateMaterial("TERRAIN_1");
		ground->DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		ground->Diffuse       = "grassColorMap";
		ground->Normal        = mMaterials.GetDefaultTextureNormalName();
	}
	{
		Material* ground = mMaterials.CreateMaterial("TERRAIN_2");
		ground->DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		ground->Diffuse       = "stoneColorMap";
		ground->Normal        = mMaterials.GetDefaultTextureNormalName();
	}
	{
		Material* ground = mMaterials.CreateMaterial("TERRAIN_3");
		ground->DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		ground->Diffuse       = "iceColorMap";
		ground->Normal        = mMaterials.GetDefaultTextureNormalName();
	}
	{
		Material* grass = mMaterials.CreateMaterial("grass_billboard_mat");
		grass->DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		grass->Diffuse       = "grassBillboardColorMap";
		grass->Normal        = mMaterials.GetDefaultTextureNormalName();
	}
	{
		Material* barkMat = mMaterials.CreateMaterial("bark_mat");
		barkMat->DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		barkMat->Diffuse       = "barkColorMap";
		barkMat->Normal        = "barkNormalMap";
		barkMat->Roughness     = 0.0f;
	}
	{
		Material* leavesMat = mMaterials.CreateMaterial("leaves_mat");
		leavesMat->DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		leavesMat->Diffuse       = "leavesColorMap";
		leavesMat->Normal        = "leavesNormalMap";
		leavesMat->Roughness     = 1.0f;
	}
	mMaterials.ReloadMaterialsCBIndices();
}

void ProTerGen::MainEngine::CreateEntites()
{
	CreateCameras();

	mMainConfig = ECS::CreateEntity();

	mRegister.RegisterComponent<MainConfig>();

	mRegister.AddComponents<MainConfig>(mMainConfig);
	MainConfig& mainConfig = mRegister.GetComponent<MainConfig>(mMainConfig);
	mainConfig.VSync     = false;
	mainConfig.Wireframe = false;
	mainConfig.Metrics   = true;
	mainConfig.OnExit    = [] { PostQuitMessage(0); };

	mRegister.RegisterSystem<MainMenuSystem>();

	CreateRenderizables();
}

void ProTerGen::MainEngine::BuildFrameResources()
{
	const uint32_t passCount = 1;
	for (int i = 0; i < gNumFrames; ++i)
	{
		mFrameResource.FrameResources.push_back(std::make_unique<FrameResources>
			(
				mDevice,
				passCount,
				mRegister.GetSystem<RenderSystem>().EntityCount(),
				mMaterials.GetMaterialCount(),
				gNumCascadeShadowMaps,
				gMaxTerrainMaterialLayers
			));
	}
}

void ProTerGen::MainEngine::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC basePsoDesc;
	ZeroMemory(&basePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	basePsoDesc.RasterizerState             = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	basePsoDesc.RasterizerState.FillMode    = D3D12_FILL_MODE_WIREFRAME;
	basePsoDesc.BlendState                  = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	basePsoDesc.DepthStencilState           = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	basePsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	basePsoDesc.SampleMask                  = UINT_MAX;
	basePsoDesc.PrimitiveTopologyType       = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	basePsoDesc.NumRenderTargets            = 1;
	basePsoDesc.RTVFormats[0]               = mBackBufferFormat;
	basePsoDesc.SampleDesc.Count            = 1;
	basePsoDesc.SampleDesc.Quality          = 0;
	basePsoDesc.DSVFormat                   = mDepthStencilFormat;

	{
		const std::vector<std::string> names =
		{
			"defaultVS",
			"defaultPS"
		};
		mShaders.CreateGraphicPSO("DefaultPSO_WF", mDevice, "DefaultRS", "DefaultInputLayout", names, basePsoDesc);
		basePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		mShaders.CreateGraphicPSO("DefaultPSO", mDevice, "DefaultRS", "DefaultInputLayout", names, basePsoDesc);
	}
	{
		const std::vector<std::string> names =
		{
			"debugVS",
			"debugPS"
		};
		mShaders.CreateGraphicPSO("DebugPSO", mDevice, "DefaultRS", "DefaultInputLayout", names, basePsoDesc);
	}
	{
		const std::vector<std::string> names =
		{
			"shadowCasterVS",
			"shadowCasterPS"
		};
		mShaders.CreateGraphicPSO("ShadowCasterPSO", mDevice, "DefaultRS", "DefaultInputLayout", names, basePsoDesc);
	}
	{
		const std::vector<std::string> names =
		{
			"terrainVS",
			"terrainHS",
			"terrainDS",
			"terrainPS"
		};
		basePsoDesc.PrimitiveTopologyType    = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
		basePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
		mShaders.CreateGraphicPSO("TerrainPSO_WF", mDevice, "DefaultRS", "DefaultInputLayout", names, basePsoDesc);
		basePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		mShaders.CreateGraphicPSO("TerrainPSO", mDevice, "DefaultRS", "DefaultInputLayout", names, basePsoDesc);
	}
	{
		const std::vector<std::string> names =
		{
			"terrainQuadVS",
			"terrainQuadHS",
			"terrainQuadDS",
			"terrainQuadPS"
		};
		basePsoDesc.PrimitiveTopologyType    = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
		basePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
		mShaders.CreateGraphicPSO("TerrainQuadPSO_WF", mDevice, "DefaultRS", "DefaultInputLayout", names, basePsoDesc);
		basePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		mShaders.CreateGraphicPSO("TerrainQuadPSO", mDevice, "DefaultRS", "DefaultInputLayout", names, basePsoDesc);
	}
	{
		const std::vector<std::string> names =
		{
			"terrainVS",
			"terrainHS",
			"shadowCasterTerrainDS",
			"shadowCasterPS"
		};
		basePsoDesc.PrimitiveTopologyType    = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
		basePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		mShaders.CreateGraphicPSO("ShadowCasterTerrainPSO", mDevice, "DefaultRS", "DefaultInputLayout", names, basePsoDesc);
	}
	{
		const std::vector<std::string> names =
		{
			"terrainVS",
			"terrainHS",
			"terrainDS",
			"terrainDebugLodPS",
		};
		basePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
		mShaders.CreateGraphicPSO("DebugLodPSO_WF", mDevice, "DefaultRS", "DefaultInputLayout", names, basePsoDesc);
		basePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		mShaders.CreateGraphicPSO("DebugLodPSO", mDevice, "DefaultRS", "DefaultInputLayout", names, basePsoDesc);
	}
	{
		const std::vector<std::string> names
		{
			"instanceBasicVS",
			"instanceBasicPS",
		};
		basePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		basePsoDesc.RasterizerState.FillMode= D3D12_FILL_MODE_WIREFRAME;
		mShaders.CreateGraphicPSO("InstanceBasicPSO_WF", mDevice, "DefaultRS", "InstanceInputLayout", names, basePsoDesc);
		basePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		mShaders.CreateGraphicPSO("InstanceBasicPSO", mDevice, "DefaultRS", "InstanceInputLayout", names, basePsoDesc);
	}
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaClipPsoDesc = basePsoDesc;
		D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc =
		{
			.BlendEnable           = true,
			.LogicOpEnable         = false,
			.SrcBlend              = D3D12_BLEND_SRC_ALPHA,
			.DestBlend             = D3D12_BLEND_INV_SRC_ALPHA,
			.BlendOp               = D3D12_BLEND_OP_ADD,
			.SrcBlendAlpha         = D3D12_BLEND_ONE,
			.DestBlendAlpha        = D3D12_BLEND_INV_SRC_ALPHA,
			.BlendOpAlpha          = D3D12_BLEND_OP_ADD,
			.LogicOp               = D3D12_LOGIC_OP_NOOP,
			.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
		};
		alphaClipPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
		const std::vector<std::string> names =
		{
			"defaultVS",
			"alphaClipPS"
		};
		alphaClipPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		alphaClipPsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
		mShaders.CreateGraphicPSO("AlphaClipPSO_WF", mDevice, "DefaultRS", "DefaultInputLayout", names, alphaClipPsoDesc);
		alphaClipPsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		mShaders.CreateGraphicPSO("AlphaClipPSO", mDevice, "DefaultRS", "DefaultInputLayout", names, alphaClipPsoDesc);
	}
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC grassPsoDesc = basePsoDesc;
		D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc =
		{
			.BlendEnable           = true,
			.LogicOpEnable         = false,
			.SrcBlend              = D3D12_BLEND_SRC_ALPHA,
			.DestBlend             = D3D12_BLEND_INV_SRC_ALPHA,
			.BlendOp               = D3D12_BLEND_OP_ADD,
			.SrcBlendAlpha         = D3D12_BLEND_ONE,
			.DestBlendAlpha        = D3D12_BLEND_INV_SRC_ALPHA,
			.BlendOpAlpha          = D3D12_BLEND_OP_ADD,
			.LogicOp               = D3D12_LOGIC_OP_NOOP,
			.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
		};
		grassPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
		const std::vector<std::string> namesGrassParticles =
		{
			"grassParticlesVS",
			"grassParticlesGS",
			"grassParticlesPS",
		};
		grassPsoDesc.PrimitiveTopologyType    = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		grassPsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
		mShaders.CreateGraphicPSO("GrassParticlesPSO_WF", mDevice, "DefaultRS", "PositionOnlyInputLayout", namesGrassParticles, grassPsoDesc);
		grassPsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		mShaders.CreateGraphicPSO("GrassParticlesPSO", mDevice, "DefaultRS", "PositionOnlyInputLayout", namesGrassParticles, grassPsoDesc);
	};
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC vtPsoDesc = basePsoDesc;
		vtPsoDesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
		vtPsoDesc.DSVFormat     = DXGI_FORMAT_D32_FLOAT;
		mShaders.CreateGraphicPSO("VTFeedbackPSO", mDevice, "DefaultRS", "DefaultInputLayout", { "vtVS", "vtPS" }, vtPsoDesc);
	}
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC vtPsoDesc = basePsoDesc;
		D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc =
		{
			.BlendEnable           = true,
			.LogicOpEnable         = false,
			.SrcBlend              = D3D12_BLEND_SRC_ALPHA,
			.DestBlend             = D3D12_BLEND_INV_SRC_ALPHA,
			.BlendOp               = D3D12_BLEND_OP_ADD,
			.SrcBlendAlpha         = D3D12_BLEND_ONE,
			.DestBlendAlpha        = D3D12_BLEND_ZERO,
			.BlendOpAlpha          = D3D12_BLEND_OP_ADD,
			.LogicOp               = D3D12_LOGIC_OP_NOOP,
			.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
		};
		vtPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
		mShaders.CreateGraphicPSO("VTDebugPSO", mDevice, "DefaultRS", "DefaultInputLayout", { "vtdebugVS", "vtdebugPS" }, vtPsoDesc);
	}
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = basePsoDesc;
		skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		mShaders.CreateGraphicPSO("SkyPSO", mDevice, "DefaultRS", "DefaultInputLayout", { "skyVS", "skyPS" }, skyPsoDesc);
	}

	mShaders.FreeShadersMemory();
}


void ProTerGen::MainEngine::BuildEnvironmentSettings()
{
	Environment& env = mMainPassCB.Env;
	env.AmbientLight   = { 0.279f, 0.264f, 0.249f, 1.000f };
	env.SkyColor       = { 0.504f, 0.693f, 0.934f, 1.000f };
	env.VanishingColor = { 0.586f, 0.665f, 0.708f, 1.000f };
	env.HorizonColor   = { 0.763f, 0.862f, 0.898f, 1.000f };
	env.CloudColor     = { 1.000f, 1.000f, 1.000f, 0.164f };
	env.FogColor       = { 0.743f, 0.853f, 0.965f, 0.663f };
	env.FogOpacity     = 1.0f;
	env.CloudOffset    = { 0.0f, 0.0f };
	env.CloudScale     = 0.8f;
	
	Light& sun = mMainPassCB.Sun;
	sun._Pad0     = 0.0f;
	sun._Pad1     = NUMBER_PI * 0.5;
	sun.Direction = { 0.0f, -1.0f, 0.0f };
	sun.Strength  = { 1.000f, 0.893f, 0.681f };
}

void ProTerGen::MainEngine::BuildCSM()
{
	const CameraComponent& camComp = mRegister.GetSystemAs<PerspectiveVisualizationSystem>().MainCameraSettings();
	const Light& sun = mMainPassCB.Sun;
	mCSM.SetCamera(camComp);
	mCSM.SetSun(sun);
	mCSM.SetFrameAddress(&mFrameResource.CurrFrameResource);
	mCSM.SetPerShadowBufferShadowRegister(5);

	const uint32_t shadowMapRes = 1024;
	mCSM.Init(mDevice, mDescriptorHeaps, shadowMapRes);
}

void ProTerGen::MainEngine::BuildMenus()
{
	MainMenuSystem& mainMenuSystem = mRegister.GetSystemAs<MainMenuSystem>();
	{
		MainConfig& mcfg = mRegister.GetComponent<MainConfig>(mMainConfig);
		// Terrain menus
		TerrainSettings& tcTer = mRegister.GetComponent<TerrainQTComponent>(mTerrain).TerrainSettings;
		EntryFunctor* ef = new EntryFunctor([&]()
			{
				ImGui::Separator();
				if (!ImGui::CollapsingHeader("Height Layers"))
					return;

				if (tcTer.Layers.size() < gMaxComputeLayers)
				{
					if (ImGui::Button("+"))
					{
						tcTer.Layers.push_back(Layer{});
					}
				}
				if (tcTer.Layers.size() > 1)
				{
					if (tcTer.Layers.size() < gMaxComputeLayers) ImGui::SameLine();
					if (ImGui::Button("-"))
					{
						tcTer.Layers.pop_back();
					}
				}
				int32_t remove = -1;
				for (size_t i = 0; i < tcTer.Layers.size(); ++i)
				{
					ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
					bool nodeOpen = ImGui::TreeNodeEx(("Layer " + std::to_string(i)).c_str(), node_flags);
					if (nodeOpen)
					{
						if (ImGui::BeginDragDropSource())
						{
							ImGui::SetDragDropPayload("LAYER", &i, sizeof(size_t));
							ImGui::EndDragDropSource();
						}
						if (ImGui::BeginDragDropTarget())
						{
							if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("LAYER"))
							{
								size_t payload_i = *(const size_t*)payload->Data;
								Layer layer = tcTer.Layers[i];
								tcTer.Layers[i] = tcTer.Layers[payload_i];
								tcTer.Layers[payload_i] = layer;
							}
							ImGui::EndDragDropTarget();
						}
						Layer& layer = tcTer.Layers[i];
						LayerDesc* elem = &layer.layerDesc;
						ImGui::SliderInt  ("Noise",        (int32_t*)elem,               0, (int32_t)LayerDesc::COUNT - 1, LayerDescNames[(int32_t)*elem].c_str());
						ImGui::SliderFloat("Seed",         &layer.seed,              1.000f, 5000.0f, "%0.3f");
						ImGui::SliderFloat("Sharpness",    &layer.amplitude,         0.010f,   10.0f, "%0.3f");
						ImGui::SliderFloat("Frecuency",    &layer.frecuency,         0.010f,    3.0f, "%0.3f");
						ImGui::SliderFloat("Gain",         &layer.gain,              0.001f,    1.0f, "%0.3f");
						ImGui::SliderInt  ("Octaves",      (int32_t*)&layer.octaves,      1,      10);
						ImGui::SliderFloat("Weight",       &layer.weight,            0.000f,    1.0f, "%0.5f");
						if (ImGui::Button("Delete Layer"))
						{
							remove = (int32_t)i;
						}
						ImGui::TreePop();
					}
				}
				if(remove > -1 && tcTer.Layers.size() > 1)
				{
					tcTer.Layers.erase(tcTer.Layers.begin() + remove);
				}
				ImGui::Separator();
				mcfg.ReloadTerrain = ImGui::Button("Regenerate");
			});

		EntryFunctor* mlef = new EntryFunctor([&]()
			{
				ImGui::Separator();
				if (!ImGui::CollapsingHeader("Material Layers"))
					return;

				if (tcTer.MaterialLayers.size() < gMaxTerrainMaterialLayers)
				{
					if (ImGui::Button("+"))
					{
						tcTer.MaterialLayers.push_back(MaterialLayer{});
						MaterialLayer& ml = tcTer.MaterialLayers.back();
						ml.material = mMaterials.GetDefaultMaterial();
					}
				}
				if (tcTer.MaterialLayers.size() > 1)
				{
					if (tcTer.MaterialLayers.size() < gMaxTerrainMaterialLayers) ImGui::SameLine();
					if (ImGui::Button("-"))
					{
						tcTer.MaterialLayers.pop_back();
					}
				}
				const std::vector<std::string> materialNames = mMaterials.GetMaterialNames();
				for (size_t materialLayerIndex = 0; materialLayerIndex < tcTer.MaterialLayers.size(); ++materialLayerIndex)
				{
					MaterialLayer& matLayer = tcTer.MaterialLayers[materialLayerIndex];
					Material& mat = *matLayer.material;
					int32_t selectedIndex = -1;
					ImGui::Separator();
					if (ImGui::BeginCombo(("Material " + std::to_string(materialLayerIndex)).c_str(), mat.Name.c_str(), 0))
					{
						for (size_t i = 0; i < materialNames.size(); ++i)
						{
							const bool isSelected = (selectedIndex == i);
							if (ImGui::Selectable(materialNames[i].c_str(), isSelected))
							{
								selectedIndex = (int32_t)i;
							}
							if (isSelected)
							{
								ImGui::SetItemDefaultFocus();
							}
						}
						ImGui::EndCombo();
					}
					if (selectedIndex > -1 && mat.Name != materialNames[selectedIndex])
					{
						matLayer.material = mMaterials.GetMaterial(materialNames[selectedIndex]);
					}
					ImGui::SliderFloat(("Min Slope " + std::to_string(materialLayerIndex)).c_str(),        &matLayer.minSlope,         0.0f, 1.0f, "%.3f");
					ImGui::SliderFloat(("Slope Smoothness " + std::to_string(materialLayerIndex)).c_str(), &matLayer.slopeSmoothness,  0.0f, 1.0f, "%.3f");
					ImGui::SliderFloat(("Min Height "+ std::to_string(materialLayerIndex)).c_str(),        &matLayer.minHeight,        0.0f, 1.0f, "%.3f");
					ImGui::SliderFloat(("Height Smoothness "+ std::to_string(materialLayerIndex)).c_str(), &matLayer.heightSmoothness, 0.0f, 1.0f, "%.3f");
				}

			});

		EntryFunctor* hef = new EntryFunctor([&]()
			{
				int32_t h = (int32_t)tcTer.TerrainWidth;
				ImGui::SliderInt("Terrain Width", &h, 10, 16384);
				tcTer.TerrainWidth = (float)h;
			});

		std::vector<Entry> entries
		{
			Entry {.type = EntryType::FLOAT,   .name = "Height",                .data = &tcTer.Height,           .disabled = false, .slider = true, .min = 0, .max = 500 },
			Entry {.type = EntryType::CUSTOM,  .name = "Terrain Width",         .data = std::move(hef),          .disabled = false, .slider = true, .min = 1, .max = 16 },
			Entry {.type = EntryType::UINT32,  .name = "Chunks per side (Exp)", .data = &tcTer.ChunksPerSideExp, .disabled = false, .slider = true, .min = 1, .max = 16 },
			Entry {.type = EntryType::UINT32,  .name = "Quads per chunk",       .data = &tcTer.QuadsPerChunk,    .disabled = false, .slider = true, .min = 1, .max = 16 },
			Entry {.type = EntryType::UINT32,  .name = "Gpu Subdivisions",      .data = &tcTer.GpuSubdivisions,  .disabled = false, .slider = true, .min = 1, .max = 16 },
			Entry {.type = EntryType::CUSTOM , .name = "Layer",                 .data = std::move(ef),           .disabled = false, .slider = true, .min = 1, .max = 16},
			Entry {.type = EntryType::CUSTOM , .name = "Terrain Layer",         .data = std::move(mlef),         .disabled = false, .slider = true, .min = 1, .max = 16}
		};
		mainMenuSystem.RegisterCollection("Terrain Menu", entries, false);
	}
	{
		// Environment menus
		Environment& env = mMainPassCB.Env;
		Light& sun = mMainPassCB.Sun;
		EntryFunctor* ef = new EntryFunctor([&]()
			{
				float deg[2] = { sun._Pad0 * 360.0f / (2.0f * NUMBER_PI), (sun._Pad1 * 360.0f) / (2.0f * NUMBER_PI) };
				ImGui::SliderFloat("Sun yaw (degrees)", &deg[0], 0, 360);
				ImGui::SliderFloat("Sun pitch (degrees)", &deg[1], 0, 90);
				sun._Pad0 = deg[0] * 2.0f * NUMBER_PI / 360.0f;
				sun._Pad1 = deg[1] * 2.0f * NUMBER_PI / 360.0f;
				sun.Direction = 
				{ 
					std::cosf(sun._Pad1) * std::sinf(sun._Pad0),
					std::sinf(-sun._Pad1),
					std::cosf(sun._Pad1) * std::cosf(sun._Pad0),
				};
				ImGui::BeginDisabled();
				ImGui::InputFloat3("Sun dir", (float*)&sun.Direction);
				ImGui::EndDisabled();
			});
		std::vector<Entry> entries
		{
			Entry {.type = EntryType::CUSTOM, .name = "Sun",             .data = std::move(ef),       .disabled = false, .slider = false, .min = 0.0f,    .max = 0.0f },
			Entry {.type = EntryType::FLOAT3, .name = "Shadow Pos",      .data = &mCSM.shadowPos,     .disabled = false, .slider = true,  .min = -500.0f, .max = 500.0f },
			Entry {.type = EntryType::COLOR3, .name = "Sun Light",       .data = &sun.Strength,       .disabled = false, .slider = false, .min = 0.0f,    .max = 0.0f },
			Entry {.type = EntryType::COLOR4, .name = "Ambient Color",   .data = &env.AmbientLight,   .disabled = false, .slider = false, .min = 0.0f,    .max = 0.0f },
			Entry {.type = EntryType::COLOR4, .name = "Sky Color",       .data = &env.SkyColor,       .disabled = false, .slider = false, .min = 0.0f,    .max = 0.0f },
			Entry {.type = EntryType::COLOR4, .name = "Vanishing Color", .data = &env.VanishingColor, .disabled = false, .slider = false, .min = 0.0f,    .max = 0.0f },
			Entry {.type = EntryType::COLOR4, .name = "Horizong Color",  .data = &env.HorizonColor,   .disabled = false, .slider = false, .min = 0.0f,    .max = 0.0f },
			Entry {.type = EntryType::COLOR4, .name = "Cloud Color",     .data = &env.CloudColor,     .disabled = false, .slider = false, .min = 0.0f,    .max = 0.0f },
			Entry {.type = EntryType::FLOAT2, .name = "Cloud Offset",    .data = &env.CloudOffset,    .disabled = false, .slider = true,  .min = -100.0f, .max = 100.0f},
			Entry {.type = EntryType::FLOAT,  .name = "Cloud Size",      .data = &env.CloudScale,     .disabled = false, .slider = true,  .min = 0.001f,  .max = 1.0f },
			Entry {.type = EntryType::COLOR4, .name = "Fog Color",       .data = &env.FogColor,       .disabled = false, .slider = false, .min = 0.0f,    .max = 0.0f },
		};
		mainMenuSystem.RegisterCollection("Environment", entries, false);
	}
	{
		// Materials menus
		EntryFunctor* ef = new EntryFunctor([&]()
			{
				const char                                     defaultCharArr[]  = " ";
				const size_t                                   numTexPerMaterial = 2;
				static std::array<char[32], numTexPerMaterial> charArrNames      = { " ", " " };
				static int32_t                                 matSelected       = 0;
				static int32_t                                 texOverwrittenIdx = 0;

				ImGui::Text("Material:");
				std::vector<std::string> matNames = mMaterials.GetMaterialNames();
				if (ImGui::BeginCombo("Material", matNames[matSelected].c_str(), ImGuiComboFlags_None))
				{
					for (size_t i = 0; i < matNames.size(); ++i)
					{
						const bool isSelected = (matSelected == (int32_t)i);
						if (ImGui::Selectable(matNames[i].c_str(), isSelected))
						{
							matSelected = (int32_t)i;
							if (isSelected)
							{
								ImGui::SetItemDefaultFocus();
							}
							for (size_t numTexIdx = 0; numTexIdx < numTexPerMaterial; ++numTexIdx)
							{
								strcpy_s(charArrNames[numTexIdx], defaultCharArr);
							}
						}
					}
					ImGui::EndCombo();
				}
				ImGui::Separator();
				bool disabledMat = false;
				if (matNames[matSelected] == mMaterials.GetDefaultMaterialName())
				{
					ImGui::BeginDisabled();
					disabledMat = true;
				}
				Material& mat    = *mMaterials.GetMaterial(matNames[matSelected]);
				Material  matCpy = mat;
				Texture&  dif    = *mMaterials.GetTexture(mat.Diffuse);
				Texture&  nor    = *mMaterials.GetTexture(mat.Normal);
				const std::array<std::string, numTexPerMaterial> namesPerMat
				{
					dif.Name,
					nor.Name
				};
				const std::array<std::wstring, numTexPerMaterial> pathsPerMat
				{
					dif.Path,
					nor.Path,
				};
				const std::array<std::string*, numTexPerMaterial> refOfMat
				{
					&mat.Diffuse,
					&mat.Normal
				};
				const std::array<std::string, numTexPerMaterial> labelsOfMat
				{
					"Diffuse",
					"Normal"
				};
				for (size_t i = 0; i < numTexPerMaterial; ++i)
				{
					if (!ImGui::CollapsingHeader((labelsOfMat[i] + " texture").c_str())) continue;
					std::vector<std::string> texNames = mMaterials.GetTextureNames([&](const std::string& name, const Materials::TextureInfo& ti) 
						{ 
							return (ti.CanUserWrite
								&& ti.HeapTable == mMaterials.GetTextureHeapTable(*refOfMat[i]))
								|| name == mMaterials.GetDefaultTextureColorName()
								|| name == mMaterials.GetDefaultTextureNormalName();
						});
					ImGui::Text((labelsOfMat[i] + " Texture:").c_str());
					int32_t selectedDif = -1;
					if (ImGui::BeginCombo((labelsOfMat[i] + "Textures").c_str(), namesPerMat[i].c_str(), ImGuiComboFlags_None))
					{
						for (size_t texNameIdx = 0; texNameIdx < texNames.size(); ++texNameIdx)
						{
							const bool isSelected = (selectedDif == (int32_t)texNameIdx);
							if (ImGui::Selectable(texNames[texNameIdx].c_str(), isSelected))
							{
								selectedDif = (int32_t)texNameIdx;
								if (isSelected)
								{
									ImGui::SetItemDefaultFocus();
								}
								strcpy_s(charArrNames[i], defaultCharArr);
							}
						}
						ImGui::EndCombo();
					}
					if (selectedDif > (-1) && texNames[selectedDif] != namesPerMat[i])
					{
						*refOfMat[i] = texNames[selectedDif];
					}
					if (*refOfMat[i] == mMaterials.GetDefaultTextureColorName() || *refOfMat[i] == mMaterials.GetDefaultTextureNormalName())
					{
						ImGui::Separator();
						continue;
					}
					ImGui::Text((labelsOfMat[i] + " Name:").c_str());
					if (std::string(charArrNames[i]) == defaultCharArr)
					{
						strcpy_s(charArrNames[i], (*refOfMat[i]).c_str());
					}
					ImGui::InputText(("New Name " + labelsOfMat[i]).c_str(), charArrNames[i], 32, ImGuiInputTextFlags_CharsNoBlank);
					if (ImGui::Button(("Change Name " + labelsOfMat[i]).c_str()))
					{
						mMaterials.ChangeTextureName((*refOfMat[i]), std::string(charArrNames[i]));
					}
					if (pathsPerMat[i] != L"")
					{
						ImGui::Text((labelsOfMat[i] + " Path: " + Wstring2String(pathsPerMat[i])).c_str());
						if (ImGui::Button(("Change texture " + labelsOfMat[i]).c_str()))
						{
							std::wstring file = mOpenFileExplorer();
							mMaterials.ChangeTextureFromPath(mDevice, mCommandList, mDescriptorHeaps, mMaterials.GetTexture(*refOfMat[i]), file);
						}
					}
				}
				if (ImGui::CollapsingHeader("Properties"))
				{
					ImGui::Text("Material properties:");
					ImGui::ColorEdit4("Diffuse Albedo ", (float*)(&mat.DiffuseAlbedo), ImGuiColorEditFlags_Float);
					ImGui::SliderFloat("Fresnel", (float*)(&mat.Fresnel), 0.0f, 1.0f, "%0.3f");
					ImGui::SliderFloat("Roughness", (float*)(&mat.Roughness), 0.0f, 1.0f, "%0.3f");
				}
				if (disabledMat)
				{
					ImGui::EndDisabled();
				}
				if (matCpy != mat)
				{
					mat.NumFramesDirty = gNumFrames;
				}
				ImGui::Separator();
				if (ImGui::Button("Add new material"))
				{
					mMaterials.NewMaterialDummy();
					ImGui::OpenPopup("New Material");
				}
				if (ImGui::Button("Add new texture"))
				{
					mMaterials.NewTextureDummy();
					ImGui::OpenPopup("New Texture");
				}
				ImVec2 center = ImGui::GetMainViewport()->GetCenter();
				ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
				if (ImGui::BeginPopupModal("New Material", 0, ImGuiWindowFlags_AlwaysAutoResize))
				{
					Material& matBuilder = *mMaterials.GetMaterialDummy();
					char newMatName[32] = "";
					strcpy_s(newMatName, matBuilder.Name.c_str());
					ImGui::InputText("Material name", newMatName, 32, ImGuiInputTextFlags_CharsNoBlank);
					matBuilder.Name = std::string(newMatName);
					ImGui::ColorEdit4 ("Diffuse Albedo ", (float*)(&matBuilder.DiffuseAlbedo), ImGuiColorEditFlags_Float);
					ImGui::SliderFloat("Fresnel",         (float*)(&matBuilder.Fresnel),   0.0f, 1.0f, "%0.3f");
					ImGui::SliderFloat("Roughness",       (float*)(&matBuilder.Roughness), 0.0f, 1.0f, "%0.3f");
					ImGui::Separator();
					if (ImGui::Button("Close"))
					{
						ImGui::CloseCurrentPopup();
					}
					ImGui::SameLine();
					if (ImGui::Button("Create"))
					{
						Material& createdMaterial = *mMaterials.CreateMaterial(matBuilder.Name);
						createdMaterial = matBuilder;
						MainConfig& mainConfig = mRegister.GetComponent<MainConfig>(mMainConfig);
						mainConfig.ResizeMaterials = true;
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndPopup();
				}
				ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
				if (ImGui::BeginPopupModal("New Texture", 0, ImGuiWindowFlags_AlwaysAutoResize))
				{
					Materials::TextureInfo& texDummy = *mMaterials.GetTextureDummy();
					char newTexName[32] = "";
					strcpy_s(newTexName, texDummy.TexturePtr->Name.c_str());
					ImGui::InputText("Texture name", newTexName, 32, ImGuiInputTextFlags_CharsNoBlank);
					const std::string newTexNameStr(newTexName);
					const bool isNameInvalid = mMaterials.TextureNameExists(newTexNameStr) || newTexNameStr == "";
					if (isNameInvalid)
					{
						ImGui::TextColored(ImVec4(1.0f, 0.475f, 0.0f, 1.0f), "INVALID NAME. (Empty or repeated) Choose a different name.");
					}
					else
					{
						texDummy.TexturePtr->Name = std::string(newTexName);
					}
					ImGui::Separator();
					if (ImGui::Button("Select file"))
					{
						std::wstring file = mOpenFileExplorer();
						texDummy.TexturePtr->Path = file;
					}
					ImGui::Text(("Path: " + Wstring2String(texDummy.TexturePtr->Path)).c_str());
					ImGui::Separator();
					
					uint32_t idx = 0;
					const std::string heapActualNames[2] = { "TERRAIN_TEXTURES", "TEXTURES" };
					for (uint32_t heapActualIdx = 0; heapActualIdx < 2; ++heapActualIdx)
					{
						if (texDummy.HeapTable == heapActualNames[heapActualIdx])
						{
							idx = heapActualIdx;
							break;
						}
					}
					const char* texHeapTypes[2] = { "Terrain", "Object" };
					ImGui::Text("Texture application: ");
					if (ImGui::BeginCombo("Texture applications", texHeapTypes[idx], ImGuiComboFlags_None))
					{
						for (size_t heapIdx = 0; heapIdx < 2; ++heapIdx)
						{
							const bool isSelected = (idx == (int32_t)heapIdx);
							if (ImGui::Selectable(texHeapTypes[heapIdx], isSelected))
							{
								idx = (int32_t)heapIdx;
								if (isSelected)
								{
									ImGui::SetItemDefaultFocus();
								}
							}
						}
						ImGui::EndCombo();
					}
					const bool isTableFull = mDescriptorHeaps.IsSrvTableFull(heapActualNames[idx]);
					if (isTableFull)
					{
						ImGui::TextColored(ImVec4(1.0f, 0.475f, 0.0f, 1.0f), "WARNING. Maximum number of textures loaded. This texture will overwrite an existing one.");
					}
					texDummy.HeapTable = heapActualNames[idx];
					ImGui::Separator();
					if (ImGui::Button("Close"))
					{
						ImGui::CloseCurrentPopup();
					}
					ImGui::SameLine();
					bool isDisabled = false;
					const bool isPathInvalid = texDummy.TexturePtr->Path == L"";
					if (isPathInvalid || isNameInvalid)
					{
						ImGui::BeginDisabled();
						isDisabled = true;
					}
					if (ImGui::Button("Create"))
					{
						if (isTableFull)
						{
							ImGui::OpenPopup("Overwrite texture");
						}
						else
						{
							mMaterials.CreateTexture(mDevice, mCommandList, texDummy.TexturePtr->Name, texDummy.TexturePtr->Path, true);
							const std::vector<std::string> toCreateNames = { texDummy.TexturePtr->Name };
							mMaterials.InsertTexturesInSRVHeap(mDevice, mDescriptorHeaps, texDummy.HeapTable, toCreateNames);
						}
						ImGui::CloseCurrentPopup();
					}
					if (isDisabled) ImGui::EndDisabled();
					ImGui::EndPopup();
				}
				ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
				if (ImGui::BeginPopupModal("Overwrite texture", 0, ImGuiWindowFlags_AlwaysAutoResize))
				{
					ImGui::Text("Select the texture that will be overwritten:");
					Materials::TextureInfo& texDummy = *mMaterials.GetTextureDummy();
					std::vector<std::string> texNames = mMaterials.GetTextureNames([&](const std::string& name, const Materials::TextureInfo& ti)
						{
							return texDummy.HeapTable == ti.HeapTable;
						});

					if (ImGui::BeginCombo("Textures to overwrite", texNames[texOverwrittenIdx].c_str(), ImGuiComboFlags_None))
					{
						for (size_t texIdx = 0; texIdx < texNames.size(); ++texIdx)
						{
							const bool isSelected = texIdx == texOverwrittenIdx;
							if (ImGui::Selectable(texNames[texIdx].c_str(), isSelected))
							{
								texOverwrittenIdx = (int32_t)texIdx;
								if (isSelected)
								{
									ImGui::SetItemDefaultFocus();
								}
							}
						}
						ImGui::EndCombo();
					}
					if (ImGui::Button("Cancel"))
					{
						ImGui::CloseCurrentPopup();
					}
					ImGui::SameLine();
					if (ImGui::Button("Overwrite"))
					{
						mMaterials.CreateTexture(mDevice, mCommandList, texDummy.TexturePtr->Name, texDummy.TexturePtr->Path, true);
						mMaterials.OverwriteTextureInSrvHeap(mDevice, mDescriptorHeaps, texNames[texOverwrittenIdx], texDummy.TexturePtr->Name);
						ImGui::CloseCurrentPopup();
					}
				}
			});
		std::vector<Entry> entries
		{
			Entry {.type = EntryType::CUSTOM, .name = "Materials", .data = std::move(ef), .disabled = false, .slider = false, .min = 0, .max = 0 },
		};
		mainMenuSystem.RegisterCollection("Materials", entries, false);
	}
}

void ProTerGen::MainEngine::InitImgui(HWND winHandle)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(winHandle);

	const uint32_t                    index     = mDescriptorHeaps.SetSrvIndex("TEXTURES_IMGUI", "Imgui_tex0", 0);
	const D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = mDescriptorHeaps.GetSrvHeap().GetCPUHandle(index);
	const D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = mDescriptorHeaps.GetSrvHeap().GetGPUHandle(index);
	ImGui_ImplDX12_Init
	(
		mDevice.Get(),
		gNumFrames,
		mBackBufferFormat,
		nullptr,
		cpuHandle,
		gpuHandle
	);

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->AddFontFromFileTTF((gFontsPath + "Rubik-Regular.ttf").c_str(), 15);
}

void ProTerGen::MainEngine::CreateCameras()
{
	const ECS::Entity mainCamera = ECS::CreateEntity();

	mRegister.RegisterComponent<FPSComponent>();
	mRegister.RegisterComponent<CameraComponent>();

	mRegister.AddComponents<FPSComponent, CameraComponent>(mainCamera);

	CameraComponent& mainCameraSettings = mRegister.GetComponent<CameraComponent>(mainCamera);
	mainCameraSettings.Position = DirectX::XMFLOAT3(0.0f, 400.0f, -1.0f);
	mainCameraSettings.Near     = 0.001f;
	mainCameraSettings.Far      = 8000.0;
	mainCameraSettings.Height   = mHeight;
	mainCameraSettings.Width    = mWidth;

	CameraFPSSystem& fps = mRegister.RegisterSystem<CameraFPSSystem>();
	PerspectiveVisualizationSystem& pvs = mRegister.RegisterSystem<PerspectiveVisualizationSystem>(mMainPassCB);

	fps.SetCamera(mainCamera);
	pvs.SetCurrentCamera(mainCamera);
}

void ProTerGen::MainEngine::CreateRenderizables()
{
	// Order: register components -> register entities -> register systems.

	mRegister.RegisterComponent<MeshRendererComponent>();
	mRegister.RegisterComponent<TerrainQTComponent>();
	mRegister.RegisterComponent<TerrainQTQuadComponent>();
	mRegister.RegisterComponent<DynamicParticleComponent>();

	uint32_t objIdx = 0;
	{
		const ECS::Entity skyEntity = ECS::CreateEntity();
		mRegister.AddComponents<MeshRendererComponent>(skyEntity);
		MeshRendererComponent& mrcSky = mRegister.GetComponent<MeshRendererComponent>(skyEntity);

		DirectX::XMStoreFloat4x4(&mrcSky.World, DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f));
		mrcSky.TexTransform    = MathHelper::Identity4x4();
		mrcSky.ObjCBIndex      = objIdx++;
		mrcSky.Mat             = mMaterials.GetDefaultMaterial();
		mrcSky.MeshGpuLocation = "sky_mesh";
		mrcSky.SubmeshLocation = "sky_dome";
		mrcSky.Layer           = (uint32_t)RenderLayer::Sky;
	}

	{
		const ECS::Entity planeEntity = ECS::CreateEntity();
		mRegister.AddComponents<MeshRendererComponent>(planeEntity);
		MeshRendererComponent& mrcPlane = mRegister.GetComponent<MeshRendererComponent>(planeEntity);

		mrcPlane.World           = MathHelper::Identity4x4();
		mrcPlane.TexTransform    = MathHelper::Identity4x4();
		mrcPlane.ObjCBIndex      = objIdx++;
		mrcPlane.Mat             = mMaterials.GetDefaultMaterial();
		mrcPlane.MeshGpuLocation = "vtdebug_quad";
		mrcPlane.SubmeshLocation = "quad";
		mrcPlane.Layer           = (uint32_t)RenderLayer::VT_Debug;
	}
	{
		mTerrain = ECS::CreateEntity();
		mRegister.AddComponents<MeshRendererComponent, TerrainQTComponent>(mTerrain);

		MeshRendererComponent& mrcTer = mRegister.GetComponent<MeshRendererComponent>(mTerrain);
		mrcTer.World        = MathHelper::Identity4x4();
		mrcTer.TexTransform = MathHelper::Identity4x4();
		mrcTer.ObjCBIndex   = objIdx++;
		mrcTer.Mat          = mMaterials.GetMaterial("TERRAIN_0");
		mrcTer.Layer        = (uint32_t)RenderLayer::OpaqueTerrain;

		TerrainQTComponent& tcTer = mRegister.GetComponent<TerrainQTComponent>(mTerrain);
		tcTer.TerrainSettings.TerrainWidth     = 8 * 1024;
		tcTer.TerrainSettings.Height           = 300.0f;
		tcTer.TerrainSettings.ChunksPerSideExp = mVTTerrainDesc.VTTilesPerRowExp;
		tcTer.TerrainSettings.QuadsPerChunk    = 3;
		tcTer.TerrainSettings.GpuSubdivisions  = 10;
		tcTer.TerrainSettings.Layers.push_back (Layer
			{
				.amplitude = 10.0f,
				.frecuency = 1.318f,
				.gain      = 0.124f,
				.octaves   = 7,
				.seed      = 3340.496f,
				.weight    = 1.0f,
			});
		tcTer.TerrainSettings.Layers.push_back (Layer
			{
				.amplitude = 10.0f,
				.frecuency = 1.222f,
				.gain      = 0.519f,
				.octaves   = 5,
				.seed      = 3586.348f,
				.weight    = 0.0901f,
			});
		tcTer.TerrainSettings.MaterialLayers.push_back(MaterialLayer
			{
				.material         = mMaterials.GetMaterial("TERRAIN_0"),
				.minSlope         = 0.0f,
				.slopeSmoothness  = 1.0f,
				.minHeight        = 0.0f,
				.heightSmoothness = 1.0f,
			});
		tcTer.TerrainSettings.MaterialLayers.push_back(MaterialLayer
			{
				.material         = mMaterials.GetMaterial("TERRAIN_1"),
				.minSlope         = 0.0f,
				.slopeSmoothness  = 1.0f,
				.minHeight        = 0.18f,
				.heightSmoothness = 0.14f,
			});
		tcTer.TerrainSettings.MaterialLayers.push_back(MaterialLayer
			{
				.material         = mMaterials.GetMaterial("TERRAIN_2"),
				.minSlope         = 0.8f,
				.slopeSmoothness  = 1.0f,
				.minHeight        = 0.0f,
				.heightSmoothness = 0.0f,
			});
		tcTer.TerrainSettings.MaterialLayers.push_back(MaterialLayer
			{
				.material         = mMaterials.GetMaterial("TERRAIN_3"),
				.minSlope         = 0.0f,
				.slopeSmoothness  = 1.0f,
				.minHeight        = 0.7f,
				.heightSmoothness = 1.0f,
			});

		mVTTerrain.GetGenerator().SetLayers(tcTer.TerrainSettings.Layers);

		const ECS::Entity grassParticle = ECS::CreateEntity();
		mRegister.AddComponents<MeshRendererComponent, DynamicParticleComponent>(grassParticle);

		MeshRendererComponent& mrcGrass = mRegister.GetComponent<MeshRendererComponent>(grassParticle);
		mrcGrass.World        = MathHelper::Identity4x4();
		mrcGrass.TexTransform = MathHelper::Identity4x4();
		mrcGrass.ObjCBIndex   = objIdx++;
		mrcGrass.Mat          = mMaterials.GetMaterial("grass_billboard_mat");
		mrcGrass.Layer        = (uint32_t)RenderLayer::GrassParticles;

		tcTer.ParticleSystems.push_back(grassParticle);
	}
	{
		const ECS::Entity instanceEntity = ECS::CreateEntity();
		mRegister.AddComponents<MeshRendererComponent>(instanceEntity);
		MeshRendererComponent& mrcIns = mRegister.GetComponent<MeshRendererComponent>(instanceEntity);
		DirectX::XMStoreFloat4x4(&mrcIns.World, DirectX::XMMatrixScaling(10.0f, 10.0f, 10.0f));
		mrcIns.TexTransform    = MathHelper::Identity4x4();
		mrcIns.ObjCBIndex      = objIdx++;
		mrcIns.Mat             = mMaterials.GetDefaultMaterial();
		mrcIns.MeshGpuLocation = "instance_cubes_mesh";
		mrcIns.SubmeshLocation = "instance_cubes";
		mrcIns.Layer = (uint32_t)RenderLayer::InstanceBasic;
	}
	{
		const ECS::Entity readObj = ECS::CreateEntity();
		mRegister.AddComponents<MeshRendererComponent>(readObj);
		MeshRendererComponent& mrcRO = mRegister.GetComponent<MeshRendererComponent>(readObj);
		DirectX::XMStoreFloat4x4(&mrcRO.World, DirectX::XMMatrixTranslation(0.0f, 0.0f, 0.0f));
		mrcRO.TexTransform    = MathHelper::Identity4x4();
		mrcRO.ObjCBIndex      = objIdx++;
		mrcRO.Mat             = mMaterials.GetMaterial("bark_mat");
		mrcRO.MeshGpuLocation = "simple_tree";
		mrcRO.SubmeshLocation = "simple_tree_log";
		mrcRO.Layer           = (uint32_t)RenderLayer::Opaque;
	}
	{
		const ECS::Entity readObj = ECS::CreateEntity();
		mRegister.AddComponents<MeshRendererComponent>(readObj);
		MeshRendererComponent& mrcRO = mRegister.GetComponent<MeshRendererComponent>(readObj);
		DirectX::XMStoreFloat4x4(&mrcRO.World, DirectX::XMMatrixTranslation(0.0f, 0.0f, 0.0f));
		mrcRO.TexTransform    = MathHelper::Identity4x4();
		mrcRO.ObjCBIndex      = objIdx++;
		mrcRO.Mat             = mMaterials.GetMaterial("leaves_mat");
		mrcRO.MeshGpuLocation = "simple_tree";
		mrcRO.SubmeshLocation = "simple_tree_leaves";
		mrcRO.Layer           = (uint32_t)RenderLayer::AlphaClip;
	}
	{
		const ECS::Entity planeAlt = ECS::CreateEntity();
		mTerrainAlt = planeAlt;
		mRegister.AddComponents<MeshRendererComponent, TerrainQTQuadComponent>(planeAlt);
		MeshRendererComponent& mrcPA = mRegister.GetComponent<MeshRendererComponent>(planeAlt);
		DirectX::XMStoreFloat4x4(&mrcPA.World, DirectX::XMMatrixTranslation(0.0f, 0.0f, 0.0f));
		mrcPA.TexTransform = MathHelper::Identity4x4();
		mrcPA.ObjCBIndex   = objIdx++;
		mrcPA.Mat          = mMaterials.GetDefaultMaterial();
		mrcPA.Layer        = (uint32_t)RenderLayer::OpaqueTerrainQuad;

		TerrainQTQuadComponent& tqc = mRegister.GetComponent<TerrainQTQuadComponent>(planeAlt);
		tqc.height           = 0.0f;
		tqc.chunksPerSideExp = 4;
		tqc.numCpuDivisions  = 1;
		tqc.numGpuDivisions  = 1;
		tqc.side             = 256.0f;
	}

	CameraComponent& camComp = mRegister.GetSystemAs<PerspectiveVisualizationSystem>().MainCameraSettings();
	mRegister.RegisterSystem<DynamicGrassParticleSystem>(mMeshes);
	mRegister.RegisterSystem<TerrainQTMorphSystem>(mMeshes, camComp, mVTTerrainDesc);
	mRegister.RegisterSystem<TerrainQTQuadSystem>(&camComp, &mMeshes);
	mRegister.RegisterSystem<RenderSystem>(&mFrameResource.CurrFrameResource, (size_t)RenderLayer::Count, mMeshes);
}

void ProTerGen::MainEngine::OnResize()
{
	App::OnResize();

	mRegister.GetSystemAs<PerspectiveVisualizationSystem>().Resize(mWidth, mHeight);
}

void ProTerGen::MainEngine::Update(const Clock& gt)
{
	mFrameResource.CurrFrameResourceIndex = (mFrameResource.CurrFrameResourceIndex + 1) % gNumFrames;
	mFrameResource.CurrFrameResource      = mFrameResource.FrameResources[mFrameResource.CurrFrameResourceIndex].get();
	mCommandAllocator                     = mFrameResource.CurrFrameResource->CommandAllocator;
	if (mFrameResource.CurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mFrameResource.CurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEvent(nullptr, false, false, nullptr);
		if (eventHandle != 0)
		{
			ThrowIfFailed(mFence->SetEventOnCompletion(mFrameResource.CurrFrameResource->Fence, eventHandle));
			WaitForSingleObject(eventHandle, INFINITE);
			CloseHandle(eventHandle);
		}
	}
	OpenCommandList();

	mMaterials.CleanTextureGarbage();

	MainConfig& mainConfig = mRegister.GetComponent<MainConfig>(mMainConfig);

	mRegister.GetSystem<CameraFPSSystem>().Update(gt.DeltaTime());
	mRegister.GetSystem<PerspectiveVisualizationSystem>().Update(gt.DeltaTime());

	mCSM.Update(gt.DeltaTime());

	UpdateImgui(gt);

	bool doubleFlush = false;
	if (mainConfig.ResizeMaterials)
	{
		if (!doubleFlush)
		{
			FlushCommandQueue();
			doubleFlush = true;
		}
		size_t materialCount = mMaterials.GetMaterialCount();
		for (uint32_t i = 0; i < gNumFrames; ++i)
		{
			mFrameResource.FrameResources[i]->MaterialBuffer->Resize(mDevice, materialCount);
		}
		mMaterials.ReloadMaterialsCBIndices();
		mainConfig.ResizeMaterials = false;
	}

	UpdateObjectCBs(gt);
	UpdateStructuredBuffers(gt);
	UpdateConstantBuffers(gt);

	if (mainConfig.ReloadTerrain)
	{
		if (!doubleFlush)
		{
			FlushCommandQueue();
			doubleFlush = true;
		}
		TerrainSettings& ts = mRegister.GetComponent<TerrainQTComponent>(mTerrain).TerrainSettings;
		mVTTerrain.GetGenerator().SetLayers(ts.Layers);
		mVTTerrain.Clear(mDevice, mDescriptorHeaps);
		mainConfig.ClearVTCache = false; // This section also cleans memory
		mainConfig.ReloadTerrain = false;
	}
	if (mainConfig.ClearVTCache)
	{
		if (!doubleFlush)
		{
			FlushCommandQueue();
		}
		mVTTerrain.Clear(mDevice, mDescriptorHeaps);
		mainConfig.ClearVTCache = false;
	}
	if (mainConfig.UpdateTerrain)
	{
		TerrainQTMorphSystem& terrSystem = mRegister.GetSystemAs<TerrainQTMorphSystem>();
		terrSystem.Update(gt.DeltaTime());
		terrSystem.UpdateOnGpu(mDevice, mCurrBackBuffer);
		DynamicGrassParticleSystem& grassSystem = mRegister.GetSystemAs<DynamicGrassParticleSystem>();
		grassSystem.Update(gt.DeltaTime());
		grassSystem.UpdateOnGpu(mDevice, mCurrBackBuffer);
		TerrainQTQuadSystem& ts = mRegister.GetSystemAs<TerrainQTQuadSystem>();
		ts.Update(gt.DeltaTime());
		ts.UpdateOnGpu(mDevice, mCurrBackBuffer);
	}
	if (mainConfig.DrawBorderColor != mVTTerrain.GetPageLoader().IsShowBordersEnabled())
	{
		mVTTerrain.GetPageLoader().EnableShowBorders(mainConfig.DrawBorderColor);
		mainConfig.ClearVTCache = true;
	}
}

void ProTerGen::MainEngine::UpdateImgui(const Clock& gt)
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	mRegister.GetSystem<MainMenuSystem>().Update(gt.DeltaTime());

	ImGui::Render();
}

void ProTerGen::MainEngine::UpdateObjectCBs(const Clock& gt)
{
	mRegister.GetSystem<RenderSystem>().Update(gt.DeltaTime());
}

void ProTerGen::MainEngine::UpdateStructuredBuffers(const Clock& gt)
{
	mMaterials.FillMaterialsBuffer(mFrameResource.CurrFrameResource->MaterialBuffer.get());
	mCSM.FillShadowStructuredBuffer(mFrameResource.CurrFrameResource->ShadowSplitBuffer.get());
	mRegister.GetSystemAs<TerrainQTMorphSystem>().FillTerrainMaterialLayersStructuredBuffer(mFrameResource.CurrFrameResource->TerrainMaterialLayerBuffer.get());
}

void ProTerGen::MainEngine::UpdateConstantBuffers(const Clock& gt)
{
	auto currPassCB = mFrameResource.CurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);

	// TODO: Change name to terrain height VT constants or make a multibuffer or something like that.
	VirtualTextureConstants vtConstants =
	{
	   .VTSize         = (float)mVTTerrainDesc.VTSize,
	   .PageTableSize  = (float)mVTTerrainDesc.VTTilesPerRow(),
	   .AtlasScale     = (float)mVTTerrainDesc.BorderedTileSize() / (float)mVTTerrainDesc.AtlasSize(),
	   .BorderScale    = (float)(mVTTerrainDesc.TileSize()) / (float)mVTTerrainDesc.BorderedTileSize(),
	   .BorderOffset   = (float)mVTTerrainDesc.BorderSize / (float)mVTTerrainDesc.BorderedTileSize(),
	   .MipBias        = (float)mVTTerrain.GetBias(),
	   .AtlasIndex     = mVTTerrain.GetAtlasTexLocation(decltype(mVTTerrain)::GenerationTextures::NORMAL_HEIGHTMAP),
	   .PageTableIndex = mVTTerrain.GetIndirectionTexId()
	};
	auto currVTCB = mFrameResource.CurrFrameResource->VtCB.get();
	currVTCB->CopyData(0, vtConstants);

	TerrainQTComponent& tc = mRegister.GetComponent<TerrainQTComponent>(mTerrain);
	TerrainConstants terConstants =
	{
		.TerrainSize      = tc.TerrainSettings.TerrainWidth,
		.TerrianChunkSize = tc.TerrainSettings.TerrainWidth / (1 << tc.TerrainSettings.ChunksPerSideExp),
		.Height           = tc.TerrainSettings.Height,
		.Subdivision      = (float)tc.TerrainSettings.GpuSubdivisions
	};
	auto currTerCB = mFrameResource.CurrFrameResource->TerrainCB.get();
	currTerCB->CopyData(0, terConstants);

	ShadowConstants shConstants{};
	mCSM.StoreShadowConstants(shConstants);
	auto currShadowCB = mFrameResource.CurrFrameResource->ShadowCB.get();
	currShadowCB->CopyData(0, shConstants);
}

void ProTerGen::MainEngine::Draw(const Clock& gt)
{
	const MainConfig& config = mRegister.GetComponent<MainConfig>(mMainConfig);
	std::string defaultPsoName    = "DefaultPSO";
	std::string alphaClipPsoName  = "AlphaClipPSO";
	std::string terrainPsoName    = config.DebugTerrainLod ? "DebugLodPSO" : "TerrainPSO";
	std::string terrainAltPsoName = "TerrainQuadPSO";
	std::string particlesPsoName  = "GrassParticlesPSO";
	std::string instancePsoName   = "InstanceBasicPSO";
	if (config.Wireframe)
	{
		defaultPsoName    += "_WF";
		alphaClipPsoName  += "_WF";
		terrainPsoName    += "_WF";
		terrainAltPsoName += "_WF";
		particlesPsoName  += "_WF";
		instancePsoName   += "_WF";
	}
	uint32_t paramIndexTextures = 0;

	//ThrowIfFailed(mCommandAllocator->Reset());
	//ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

	mCommandList->SetDescriptorHeaps(1, mDescriptorHeaps.GetSrvHeap().Heap.GetAddressOf());
	//mGrass.Compute(mCommandList, mShaders, mDescriptorHeaps, mMainPassCB.EyePosW);
	mCommandList->SetGraphicsRootSignature(mShaders.GetRootSignature("DefaultRS").Get());
	{
		// Bind memory. This section should be the same as the parameters defined in the root signature.
		
		uint32_t paramIndex = 1; // Skip object CB;

		Microsoft::WRL::ComPtr<ID3D12Resource> passCB = mFrameResource.CurrFrameResource->PassCB->Resource();
		mCommandList->SetGraphicsRootConstantBufferView(paramIndex++, passCB->GetGPUVirtualAddress());

		Microsoft::WRL::ComPtr<ID3D12Resource> vtCB = mFrameResource.CurrFrameResource->VtCB->Resource();
		mCommandList->SetGraphicsRootConstantBufferView(paramIndex++, vtCB->GetGPUVirtualAddress());

		Microsoft::WRL::ComPtr<ID3D12Resource> terrainCB = mFrameResource.CurrFrameResource->TerrainCB->Resource();
		mCommandList->SetGraphicsRootConstantBufferView(paramIndex++, terrainCB->GetGPUVirtualAddress());

		Microsoft::WRL::ComPtr<ID3D12Resource> shadowCB = mFrameResource.CurrFrameResource->ShadowCB->Resource();
		mCommandList->SetGraphicsRootConstantBufferView(paramIndex++, shadowCB->GetGPUVirtualAddress());

		paramIndex++; // Skip per shadow CB

		Microsoft::WRL::ComPtr<ID3D12Resource> matBuffer = mFrameResource.CurrFrameResource->MaterialBuffer->Resource();
		mCommandList->SetGraphicsRootShaderResourceView(paramIndex++, matBuffer->GetGPUVirtualAddress());

		Microsoft::WRL::ComPtr<ID3D12Resource> shadowBuffer = mFrameResource.CurrFrameResource->ShadowSplitBuffer->Resource();
		mCommandList->SetGraphicsRootShaderResourceView(paramIndex++, shadowBuffer->GetGPUVirtualAddress());
		
		Microsoft::WRL::ComPtr<ID3D12Resource> terrainLayersBuffer = mFrameResource.CurrFrameResource->TerrainMaterialLayerBuffer->Resource();
		mCommandList->SetGraphicsRootShaderResourceView(paramIndex++, terrainLayersBuffer->GetGPUVirtualAddress());

		uint32_t                      location = mDescriptorHeaps.GetSrvTableEntryLocation("INDIRECTION_TEXTURES");
		CD3DX12_GPU_DESCRIPTOR_HANDLE handle   = mDescriptorHeaps.GetSrvHeap().GetGPUHandle(location);
		mCommandList->SetGraphicsRootDescriptorTable(paramIndex++, handle);

		location = mDescriptorHeaps.GetSrvTableEntryLocation("ATLAS_TEXTURES");
		handle   = mDescriptorHeaps.GetSrvHeap().GetGPUHandle(location);
		mCommandList->SetGraphicsRootDescriptorTable(paramIndex++, handle);

		paramIndexTextures = paramIndex;
		location = mDescriptorHeaps.GetSrvTableEntryLocation("TEXTURES");
		handle   = mDescriptorHeaps.GetSrvHeap().GetGPUHandle(location);
		mCommandList->SetGraphicsRootDescriptorTable(paramIndex++, handle);

		location = mDescriptorHeaps.GetSrvTableEntryLocation("CASCADE_SHADOW_MAPS");
		handle   = mDescriptorHeaps.GetSrvHeap().GetGPUHandle(location);
		mCommandList->SetGraphicsRootDescriptorTable(paramIndex++, handle);
	}

	{
		// Feedback buffer pass
		if (config.UpdateVT)
		{
		    mCommandList->SetPipelineState(mShaders.GetPSO("VTFeedbackPSO").Get());

			mFeedbackBuffer.Download();
			mVTTerrain.Update(mCommandList, mFeedbackBuffer.Requests());
			mFeedbackBuffer.SetAsWriteable(mCommandList);
			mFeedbackBuffer.Clear(mCommandList, mDescriptorHeaps);
			mFeedbackBuffer.SetAsRenderTarget(mCommandList, mDescriptorHeaps);
			mCommandList->RSSetScissorRects(1, &mScissorRect);

			DrawRenderItems(mCommandList.Get(), RenderLayer::OpaqueTerrain);

			mFeedbackBuffer.SetAsReadable(mCommandList);
			mFeedbackBuffer.Copy(mCommandList);

#ifdef QT_FEEDBACK_REQUESTS
			auto& requests = mRegister.GetSystemAs<TerrainQTMorphSystem>().GetLastRequests();
			mVTTerrain.Update(mCommandList, requests);
#endif
		}
	}

	{
		// ShadowPass
		mCSM.Clear(mCommandList, mDescriptorHeaps);
		mCSM.Draw(mCommandList, mDescriptorHeaps,
			[&]()
			{
				mCommandList->SetPipelineState(mShaders.GetPSO("ShadowCasterTerrainPSO").Get());
				DrawRenderItems(mCommandList.Get(), RenderLayer::OpaqueTerrain, D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
				mCommandList->SetPipelineState(mShaders.GetPSO("ShadowCasterPSO").Get());
				DrawRenderItems(mCommandList.Get(), RenderLayer::Opaque);
			});
	}

	{
		// Draw color pass
		mCommandList->RSSetViewports(1, &mScreenViewport);
		mCommandList->RSSetScissorRects(1, &mScissorRect);

		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition
		(
			GetCurrentRenderBuffer().Get(),
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		);
		mCommandList->ResourceBarrier(1, &barrier);

		float color[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		mCommandList->ClearRenderTargetView(GetCurrentRenderBufferView(), color, 0, nullptr);
		mCommandList->ClearDepthStencilView(GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = GetDepthStencilView();
		D3D12_CPU_DESCRIPTOR_HANDLE renderBufferView = GetCurrentRenderBufferView();
		mCommandList->OMSetRenderTargets(1, &renderBufferView, true, &depthStencilView);

		// Swap textures
		uint32_t                    location = mDescriptorHeaps.GetSrvTableEntryLocation("TERRAIN_TEXTURES");
		D3D12_GPU_DESCRIPTOR_HANDLE handle   = mDescriptorHeaps.GetSrvHeap().GetGPUHandle(location);
		mCommandList->SetGraphicsRootDescriptorTable(paramIndexTextures, handle);
		
		mCommandList->SetPipelineState(mShaders.GetPSO(terrainPsoName).Get());
		DrawRenderItems(mCommandList.Get(), RenderLayer::OpaqueTerrain, D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
		//mCommandList->SetPipelineState(mShaders.GetPSO(terrainAltPsoName).Get());
		//DrawRenderItems(mCommandList.Get(), RenderLayer::OpaqueTerrainQuad, D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST);

		// Swap textures back
		location = mDescriptorHeaps.GetSrvTableEntryLocation("TEXTURES");
		handle   = mDescriptorHeaps.GetSrvHeap().GetGPUHandle(location);
		mCommandList->SetGraphicsRootDescriptorTable(paramIndexTextures, handle);

		//mCommandList->SetPipelineState(mShaders.GetPSO(defaultPsoName).Get());
		//DrawRenderItems(mCommandList.Get(), RenderLayer::Opaque, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		//mCommandList->SetPipelineState(mShaders.GetPSO(alphaClipPsoName).Get());
		//DrawRenderItems(mCommandList.Get(), RenderLayer::AlphaClip, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		mCommandList->SetPipelineState(mShaders.GetPSO("SkyPSO").Get());
		DrawRenderItems(mCommandList.Get(), RenderLayer::Sky);

		//mCommandList->SetPipelineState(mShaders.GetPSO(instancePsoName).Get());
		//InstanceRenderItems(mCommandList.Get(), RenderLayer::InstanceBasic);

		//mCommandList->SetPipelineState(mShaders.GetPSO(particlesPsoName).Get());
		//DrawRenderItems(mCommandList.Get(), RenderLayer::GrassParticles, D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

		// Debug virtual texture draw
		if (config.DebugVT)
		{
			D3D12_VIEWPORT viewport = mScreenViewport;
			viewport.Height /= 4;
			viewport.Width  /= 2;
			viewport.TopLeftY = viewport.Height * 3;
			mCommandList->RSSetViewports(1, &viewport);
			mCommandList->SetPipelineState(mShaders.GetPSO("VTDebugPSO").Get());
			DrawRenderItems(mCommandList.Get(), RenderLayer::VT_Debug);
		}

		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mCommandList.Get());

		barrier = CD3DX12_RESOURCE_BARRIER::Transition
		(
			GetCurrentRenderBuffer().Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT
		);
		mCommandList->ResourceBarrier(1, &barrier);
	}

	{
		// Execute
		ExecuteCommandList();

		ThrowIfFailed(mSwapChain->Present(config.VSync, 0));
		mCurrBackBuffer = (mCurrBackBuffer + 1) % gNumFrames;

		mFrameResource.CurrFrameResource->Fence = ++mCurrentFence;

		mCommandQueue->Signal(mFence.Get(), mCurrentFence);
	}
}

void ProTerGen::MainEngine::DrawRenderItems(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, RenderLayer renderLayer, D3D12_PRIMITIVE_TOPOLOGY topology)
{
	const RenderContext ctx =
	{
		.CommandList = cmdList,
		.Layer       = (size_t)renderLayer,
		.Topology    = topology
	};
	mRegister.GetSystemAs<RenderSystem>().Render(ctx);
}

void ProTerGen::MainEngine::InstanceRenderItems(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, RenderLayer renderLayer, D3D12_PRIMITIVE_TOPOLOGY topology)
{
	const RenderContext ctx =
	{
		.CommandList = cmdList,
		.Layer       = (size_t)renderLayer,
		.Topology    = topology
	};
	mRegister.GetSystemAs<RenderSystem>().Instance(ctx);
}

void ProTerGen::MainEngine::ExecuteCommandList()
{
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
}

void ProTerGen::MainEngine::OpenCommandList()
{
	ThrowIfFailed(mCommandAllocator->Reset());
	ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));
}

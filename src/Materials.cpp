#include "Materials.h"

ProTerGen::Material* ProTerGen::Materials::CreateMaterial(const std::string& name)
{
	std::unique_ptr<Material> mat = std::make_unique<Material>();
	mat->Name = name;

	mMaterials.emplace(name, std::move(mat));

	return mMaterials.at(name).get();
}

size_t ProTerGen::Materials::GetMaterialCount() const
{
	return mMaterials.size();
}

ProTerGen::Material* ProTerGen::Materials::GetMaterial(const std::string& name)
{
	return mMaterials.at(name).get();
}

std::vector<ProTerGen::Material*> ProTerGen::Materials::GetMaterials() const
{
	std::vector<Material*> res{};
	for (const auto& [_, materialUniquePtr] : mMaterials)
	{
		res.push_back(materialUniquePtr.get());
	}
	return res;
}

std::vector<std::string> ProTerGen::Materials::GetMaterialNames() const
{
	std::vector<std::string> res{};
	for (const auto& [name, _] : mMaterials)
	{
		res.push_back(name);
	}
	return res;
}

ProTerGen::Material* ProTerGen::Materials::GetDefaultMaterial() const
{
	return mMaterials.at(DEFAULT_MATERIAL_NAME).get();
}

void ProTerGen::Materials::ReloadMaterialsCBIndices()
{
	int32_t i = 0;
	for (auto& [_, mat] : mMaterials)
	{
		mat->MatCBIndex     = ++i;
		mat->NumFramesDirty = gNumFrames;
	}
}

void ProTerGen::Materials::FillMaterialsBuffer(UploadBuffer<MaterialConstants>* buffer)
{
	for (auto& [_, mat] : mMaterials)
	{
		if (mGlobalDirty)
		{
			mat->NumFramesDirty = gNumFrames;
		}
		if (mat->NumFramesDirty > 0)
		{
			if (mTextures.count(mat->Diffuse) < 1)
			{
				printf("Attepting to access to texture: %s, but not loaded in memory.\n", mat->Diffuse.c_str());
				throw - 1;
			}
			if (mTextures.count(mat->Normal) < 1)
			{
				printf("Attepting to access to texture: %s, but not loaded in memory.\n", mat->Normal.c_str());
				throw - 1;
			}

			DirectX::XMMATRIX matTransform = DirectX::XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matData{};
			matData.DiffuseAlbedo = mat->DiffuseAlbedo;
			matData.Fresnel       = mat->Fresnel;
			matData.Roughness     = mat->Roughness;
			DirectX::XMStoreFloat4x4(&matData.MatTransform, DirectX::XMMatrixTranspose(matTransform));
			matData.DiffuseMapIndex = mTextures.at(mat->Diffuse).TexturePtr->Location;
			matData.NormalMapIndex  = mTextures.at(mat->Normal).TexturePtr->Location;

			buffer->CopyData(mat->MatCBIndex, matData);

			mat->NumFramesDirty--;
		}
	}
	mGlobalDirty = false;
}

ProTerGen::Texture* const ProTerGen::Materials::CreateTexture
(
	Microsoft::WRL::ComPtr<ID3D12Device> device,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
	const std::string name,
	const std::wstring path,
	bool canUserWrite
)
{
	mTextures[name] = { .Type = TextureType::TEXTURE_SRV };
	mTextures[name].TexturePtr = std::make_unique<Texture>();
	mTextures[name].TexturePtr->Name = name;
	mTextures[name].TexturePtr->Path = path;
	mTextures[name].CanUserWrite = canUserWrite;

	TextureCreator textureCreator = {};
	bool isCubeMap = false;
	textureCreator.CreateFromFile
	(
		device,
		commandList,
		mTextures[name].TexturePtr->Path,
		mTextures[name].TexturePtr->Resource,
		mTextures[name].TexturePtr->UploadHeap,
		isCubeMap
	);

	if (isCubeMap)
	{
		mTextures[name].Type = TextureType::SKYMAP_SRV;
	}

	return mTextures[name].TexturePtr.get();
}

ProTerGen::Texture* const ProTerGen::Materials::CreateEmptyTexture(const std::string& name, TextureType type, bool canUserWrite)
{
	if (mTextures.count(name) > 0) return mTextures[name].TexturePtr.get();

	mTextures[name] = { .Type = type };
	mTextures[name].TexturePtr = std::make_unique<Texture>();
	mTextures[name].TexturePtr->Name = name;
	mTextures[name].CanUserWrite = canUserWrite;

	return mTextures[name].TexturePtr.get();
}

void ProTerGen::Materials::CreateDefaultTextures(Microsoft::WRL::ComPtr<ID3D12Device> device, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList)
{
	const std::string names[]
	{
		DEFAULT_TEX_COLOR_MAP_NAME,
		DEFAULT_TEX_NORMAL_MAP_NAME
	};
	const std::wstring dxnames[]
	{
		L"materials_default_tex_color_map",
		L"materials_default_tex_color_map_up",
		L"materials_default_tex_normal_map",
		L"materials_default_tex_normal_map_up"
	};
	const uint8_t values[2][4]
	{
		{ 255, 255, 255, 255 },
		{ 127, 127, 255, 255 }
	};
	for (uint32_t i = 0; i < 2; ++i)
	{
		const std::string& name = names[i];
		mTextures[name] = { .Type = TextureType::TEXTURE_SRV };
		mTextures[name].TexturePtr = std::make_unique<Texture>();
		mTextures[name].TexturePtr->Name = name;

		Texture& tex = *mTextures[name].TexturePtr;
		const D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		const D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1);
		ThrowIfFailed(device->CreateCommittedResource
		(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(tex.Resource.ReleaseAndGetAddressOf())
		));
		tex.Resource->SetName(dxnames[i * 2].c_str());

		const D3D12_HEAP_PROPERTIES upHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		const D3D12_RESOURCE_DESC upResDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint8_t) * 4);
		ThrowIfFailed(device->CreateCommittedResource
		(
			&upHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&upResDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(tex.UploadHeap.ReleaseAndGetAddressOf())
		));
		tex.UploadHeap->SetName(dxnames[(i * 2) + 1].c_str());

		void* mapped = nullptr;
		ThrowIfFailed(tex.UploadHeap->Map(0, nullptr, &mapped));
		((uint8_t*)mapped)[0] = values[i][0];
		((uint8_t*)mapped)[1] = values[i][1];
		((uint8_t*)mapped)[2] = values[i][2];
		((uint8_t*)mapped)[3] = values[i][3];
		tex.UploadHeap->Unmap(0, nullptr);
		mapped = nullptr;

		D3D12_RESOURCE_BARRIER barriers[1] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(tex.Resource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST, 0),
		};
		cmdList->ResourceBarrier(_countof(barriers), barriers);

		const D3D12_TEXTURE_COPY_LOCATION locDst
		{
			.pResource = tex.Resource.Get(),
			.Type = D3D12_TEXTURE_COPY_TYPE::D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
			.SubresourceIndex = 0,
		};
		const D3D12_TEXTURE_COPY_LOCATION locSrc
		{
			.pResource = tex.UploadHeap.Get(),
			.Type = D3D12_TEXTURE_COPY_TYPE::D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
			.PlacedFootprint =
			{
				.Footprint =
				{
					.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
					.Width  = 1,
					.Height = 1,
					.Depth  = 1,
					.RowPitch = (uint32_t)Align(1, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1),
				}
			}
		};
		const D3D12_BOX box
		{
			.left = 0,
			.top = 0,
			.front = 0,
			.right = 1,
			.bottom = 1,
			.back = 1,
		};
		cmdList->CopyTextureRegion(&locDst, 0, 0, 0, &locSrc, &box);
		barriers[0] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(tex.Resource.Get(),  D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, 0)
		};
		cmdList->ResourceBarrier(_countof(barriers), barriers);
	}
}

ProTerGen::Texture* const ProTerGen::Materials::GetTexture(const std::string& name)
{
	if (mTextures.count(name) > 0) return mTextures.at(name).TexturePtr.get();
	return nullptr;
}

std::vector<std::string> ProTerGen::Materials::GetTextureNames(filter_func_t func) const
{
	std::vector<std::string> res{};
	for (const auto& [name, tex] : mTextures)
	{
		if (func(name, tex))
		{
			res.push_back(name);
		}
	}
	return res;
}

std::string ProTerGen::Materials::GetTextureHeapTable(const std::string& texName) const
{
	if (mTextures.count(texName) < 1) return "";
	return mTextures.at(texName).HeapTable;
}

ProTerGen::Texture* const ProTerGen::Materials::ChangeTextureFromPath
(
	Microsoft::WRL::ComPtr<ID3D12Device>              device,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList,
	DescriptorHeaps&                                  descriptorHeaps,
	Texture*                                          oldTexture,
	const std::wstring&                               newPath
)
{
	const std::string name            = oldTexture->Name;
	if (name == DEFAULT_TEX_COLOR_MAP_NAME || name == DEFAULT_TEX_NORMAL_MAP_NAME) return oldTexture;
	int32_t           textureSrvIndex = oldTexture->Location;
	AddTextureToGarbage(name);
	mTextures[name].TexturePtr        = std::make_unique<Texture>();
	mTextures[name].TexturePtr->Name  = name;
	mTextures[name].TexturePtr->Path  = newPath;

	TextureCreator textureCreator = {};
	bool isCubeMap = false;
	textureCreator.CreateFromFile
	(
		device,
		cmdList,
		mTextures[name].TexturePtr->Path,
		mTextures[name].TexturePtr->Resource,
		mTextures[name].TexturePtr->UploadHeap,
		isCubeMap
	);

	if (isCubeMap)
	{
		mTextures[name].Type = TextureType::SKYMAP_SRV;
	}

	const uint32_t                      srvTableOffset = descriptorHeaps.GetSrvTableEntryLocation(mTextures[name].HeapTable);
	const CD3DX12_CPU_DESCRIPTOR_HANDLE handle         = descriptorHeaps.GetSrvHeap().GetCPUHandle(srvTableOffset + textureSrvIndex);
	D3D12_SRV_DIMENSION                 dim            = isCubeMap ? D3D12_SRV_DIMENSION_TEXTURECUBE : D3D12_SRV_DIMENSION_TEXTURE2D;

	const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc =
	{
	   .Format                  = mTextures[name].TexturePtr->Resource->GetDesc().Format,
	   .ViewDimension           = dim,
	   .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
	   .Texture2D =
	   {
		  .MostDetailedMip     = 0,
		  .MipLevels           = mTextures[name].TexturePtr->Resource->GetDesc().MipLevels,
		  .ResourceMinLODClamp = 0.0f
	   },
	};
	device->CreateShaderResourceView(mTextures[name].TexturePtr->Resource.Get(), &srvDesc, handle);
	mTextures[name].TexturePtr->Location = textureSrvIndex;

	return mTextures[name].TexturePtr.get();
}

void ProTerGen::Materials::ChangeTextureName(const std::string oldName, const std::string newName)
{
	if (mTextures.count(oldName) < 1 || mTextures.count(newName) > 0 || oldName == newName) return;
	mTextures[newName]                  = std::move(mTextures[oldName]);
	mTextures[newName].TexturePtr->Name = newName;
	for (auto& [name, mat] : mMaterials)
	{
		if (mat->Diffuse == oldName)
		{
			mat->Diffuse = newName;
		}
		if (mat->Normal == oldName)
		{
			mat->Normal = newName;
		}
	}
	mTextures.erase(oldName);
}

void ProTerGen::Materials::OverwriteTextureInSrvHeap
(
	Microsoft::WRL::ComPtr<ID3D12Device> device,
	DescriptorHeaps&                     descriptorHeaps,
	const std::string&                   oldTextureName,
	const std::string&                   newTextureName
)
{
	if (mTextures.count(oldTextureName) < 1 || mTextures.count(newTextureName) < 1) return;
	TextureInfo& oldTexture = mTextures[oldTextureName];
	TextureInfo& newTexture = mTextures[newTextureName];

	const uint32_t textureLocationInHeap = oldTexture.TexturePtr->Location;

	const uint32_t                      srvTableOffset = descriptorHeaps.GetSrvTableEntryLocation(oldTexture.HeapTable);
	const CD3DX12_CPU_DESCRIPTOR_HANDLE handle         = descriptorHeaps.GetSrvHeap().GetCPUHandle(srvTableOffset + textureLocationInHeap);
	D3D12_SRV_DIMENSION                 dim            = newTexture.Type == TextureType::SKYMAP_SRV 
															? D3D12_SRV_DIMENSION_TEXTURECUBE : D3D12_SRV_DIMENSION_TEXTURE2D;

	const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc =
	{
	   .Format                  = newTexture.TexturePtr->Resource->GetDesc().Format,
	   .ViewDimension           = dim,
	   .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
	   .Texture2D =
	   {
		  .MostDetailedMip     = 0,
		  .MipLevels           = newTexture.TexturePtr->Resource->GetDesc().MipLevels,
		  .ResourceMinLODClamp = 0.0f
	   },
	};
	device->CreateShaderResourceView(newTexture.TexturePtr->Resource.Get(), &srvDesc, handle);
	newTexture.HeapTable            = oldTexture.HeapTable;
	newTexture.TexturePtr->Location = textureLocationInHeap;

	AddTextureToGarbage(oldTextureName);
	oldTexture.TexturePtr = nullptr;
	mTextures.erase(oldTextureName);

	for (auto& [_, mat] : mMaterials)
	{
		if (mat->Diffuse == oldTextureName)
		{
			mat->Diffuse = DEFAULT_TEX_COLOR_MAP_NAME;
		}
		if (mat->Normal == oldTextureName)
		{
			mat->Normal = DEFAULT_TEX_NORMAL_MAP_NAME;
		}
	}
}

void ProTerGen::Materials::InsertTexturesInSRVHeap
(
	Microsoft::WRL::ComPtr<ID3D12Device> device,
	DescriptorHeaps&                     descriptorHeaps,
	const std::string&                   tableEntryName,
	const std::vector<std::string>&      texNames
)
{
	const uint32_t srvTableOffset = descriptorHeaps.GetSrvTableEntryLocation(tableEntryName);
	const uint32_t offset         = descriptorHeaps.SetContinuousSrvIndices(tableEntryName, tableEntryName, (uint32_t)texNames.size());

	uint32_t count = 0;
	for (const std::string& textureName : texNames)
	{
		TextureInfo&                  texInfo       = mTextures[textureName];
		const uint32_t                descriptorIdx = offset + count;
		CD3DX12_CPU_DESCRIPTOR_HANDLE handle        = descriptorHeaps.GetSrvHeap().GetCPUHandle(descriptorIdx);
		if (texInfo.Type == TextureType::TEXTURE_UAV)
		{
			const D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc =
			{
				.Format        = texInfo.TexturePtr->Resource->GetDesc().Format,
				.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D
			};
			device->CreateUnorderedAccessView(texInfo.TexturePtr->Resource.Get(), nullptr, &uavDesc, handle);
		}
		else
		{
			D3D12_SRV_DIMENSION dim = D3D12_SRV_DIMENSION_TEXTURE2D;
			if (texInfo.Type == TextureType::SKYMAP_SRV)
			{
				dim = D3D12_SRV_DIMENSION_TEXTURECUBE;
			}
			const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc =
			{
			   .Format                  = texInfo.TexturePtr->Resource->GetDesc().Format,
			   .ViewDimension           = dim,
			   .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
			   .Texture2D =
			   {
				  .MostDetailedMip     = 0,
				  .MipLevels           = texInfo.TexturePtr->Resource->GetDesc().MipLevels,
				  .ResourceMinLODClamp = 0.0f
			   },
			};
			device->CreateShaderResourceView(texInfo.TexturePtr->Resource.Get(), &srvDesc, handle);
		}
		texInfo.HeapTable            = tableEntryName;
		texInfo.TexturePtr->Location = (int32_t)(descriptorIdx - srvTableOffset);
		++count;
	}
}

size_t ProTerGen::Materials::GetTextureCount() const
{
	return mTextures.size();
}

void ProTerGen::Materials::CleanTextureGarbage()
{
	const size_t s = mTextureGarbage.size();
	for (size_t i = 0; i < s; ++i)
	{
		if (mTextureGarbage[i].FramesLeft > 0)
		{
			--mTextureGarbage[i].FramesLeft;
		}
		else
		{
			mTextureGarbage.erase(mTextureGarbage.begin() + i, mTextureGarbage.begin() + i + 1);
		}
	}
}

ProTerGen::Material* ProTerGen::Materials::NewMaterialDummy()
{
	mMaterialDummy = std::make_unique<Material>();
	mMaterialDummy->Diffuse        = DEFAULT_TEX_COLOR_MAP_NAME;
	mMaterialDummy->Normal         = DEFAULT_TEX_NORMAL_MAP_NAME;
	mMaterialDummy->Name           = "_NEW_MATERIAL_";
	mMaterialDummy->NumFramesDirty = gNumFrames;
	return mMaterialDummy.get();
}

ProTerGen::Material* ProTerGen::Materials::GetMaterialDummy()
{
	return mMaterialDummy.get();
}

ProTerGen::Materials::TextureInfo* ProTerGen::Materials::NewTextureDummy()
{
	mTextureDummy                   = std::make_unique<TextureInfo>();
	mTextureDummy->TexturePtr       = std::make_unique<Texture>();
	mTextureDummy->TexturePtr->Name = "_NEW_TEXTURE_";
	return mTextureDummy.get();
}

ProTerGen::Materials::TextureInfo* ProTerGen::Materials::GetTextureDummy()
{
	return mTextureDummy.get();
}

void ProTerGen::Materials::AddTextureToGarbage(const std::string& texName)
{
	if (mTextures.count(texName) < 1) return;
	mTextureGarbage.push_back(TextureGarbage
		{
			.Texture    = std::move(mTextures[texName].TexturePtr),
			.FramesLeft = gNumFrames
		});
}

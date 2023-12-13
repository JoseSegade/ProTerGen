#include "SplitTexture.h"

constexpr size_t TileStride(size_t tileWidth, size_t numChannels, size_t bytesPerChannnel)
{
	return tileWidth * tileWidth * numChannels * bytesPerChannnel;
}

constexpr size_t Offset(size_t x, size_t y, uint32_t gridWidth, size_t tileWidth, size_t numChannels, size_t bytesPerChannel)
{
	return (x + y * gridWidth) * TileStride(tileWidth, numChannels, bytesPerChannel);
}

void ProTerGen::ST::TextureLoader::Init(STDesc* desc, SplitTextureInfo* info)
{
	mDesc = desc;
	mInfo = info;

	if (!OpenReadingFromPath())
	{
		CreateNewFile();
	}
}

void ProTerGen::ST::TextureLoader::CreateNewFile()
{
	const size_t texSize = (size_t)mDesc->NumTiles() * mInfo->numChannels * mInfo->bytesPerChannel;
	mWriter = _wfopen(mInfo->path.c_str(), L"wb+");
	if (mWriter == nullptr)
	{
		perror("Failed to open file.");
		return;
	}
	if (_fseeki64(mWriter, texSize - 1, SEEK_SET) != 0)
	{
		perror("Failed to seek to the desired file size.");
		fclose(mWriter);
		return;
	}
	fputc(0, mWriter);
	fflush(mWriter);
	_fseeki64(mWriter, 0, SEEK_SET);
	mFile = _wfopen(mInfo->path.c_str(), L"rb");
	if (mFile == nullptr)
	{
		perror("Failed to open file for reading.\n");
		Close();
	}
}

bool ProTerGen::ST::TextureLoader::OpenReadingFromPath()
{
	mFile = _wfopen(mInfo->path.c_str(), L"rb");
	if (mFile == nullptr) return false;
	mWriter = _wfopen(mInfo->path.c_str(), L"rb+");
	if (mWriter == nullptr)
	{
		fclose(mFile);
		return false;
	}
	return true;
}

bool ProTerGen::ST::TextureLoader::Read(PageData& page)
{
	const size_t tileSize = (size_t)mDesc->TileWidth() * mDesc->TileWidth() * mInfo->numChannels * mInfo->bytesPerChannel;
	page.data = malloc(tileSize);
	if (page.data == nullptr)
	{
		printf("Memory error while reading file.\n");
		return false;
	}
	const size_t location = Offset(page.page.x, page.page.y, mDesc->TilesPerSide(), mDesc->TileWidth(), mInfo->numChannels, mInfo->bytesPerChannel);
	_fseeki64(mFile, location, SEEK_SET);
	if (fread(page.data, 1, tileSize, mFile) != tileSize)
	{
		free(page.data);
		page.data = nullptr;
		return false;
	}
	return true;
}

bool ProTerGen::ST::TextureLoader::Write(const PageData& page)
{
	const size_t tileSize = (size_t)mDesc->TileWidth() * mDesc->TileWidth() * mInfo->numChannels * mInfo->bytesPerChannel;
	const size_t location = Offset(page.page.x, page.page.y, mDesc->TilesPerSide(), mDesc->TileWidth(), mInfo->numChannels, mInfo->bytesPerChannel);
	_fseeki64(mWriter, location, SEEK_SET);
	if (fwrite(page.data, 1, tileSize, mWriter) != tileSize)
	{
		return false;
	}
	fflush(mWriter);
	return true;
}

void ProTerGen::ST::TextureLoader::DeleteFileTexture()
{
	Close();
	if (_wremove(mInfo->path.c_str()) != 0)
	{
		perror("Unable to delete file.\n");
	}
}

void ProTerGen::ST::TextureLoader::Close()
{
	fclose(mFile);
	fclose(mWriter);
}

void ProTerGen::ST::Cache::Init(STDesc* desc)
{
	mDesc = desc;
	mLru.Resize((size_t)mDesc->AtlasNumTiles());
	mLru.OnRemove(mOnRemove);
}

bool ProTerGen::ST::Cache::UpdatePage(Page& p)
{
	if (mLoading.count(p) < 1)
	{
		Point point{};
		return mLru.TryGet(p, point, true);
	}
	return false;
}

bool ProTerGen::ST::Cache::RequestPage(Page& p)
{
	if (mLoading.count(p) < 1)
	{
		Point point{};
		if (!mLru.TryGet(p, point, false))
		{
			mLoading.insert(p);
			mSubmit(p);
			return true;
		}
	}
	return false;
}

void ProTerGen::ST::Cache::LoadComplete(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, PageData& page)
{
	mLoading.erase(page.page);
	Point point{};
	if (mCurrent == mDesc->AtlasNumTiles())
	{
		point = mLru.RemoveLast();
	}
	else
	{
		point = { .X = mCurrent % mDesc->AtlasTilesPerSide, .Y = mCurrent / mDesc->AtlasTilesPerSide };
		++mCurrent;
	}

	mUploadData(commandList, point, page.data);
	mLru.Add(page.page, point);
	mOnPageAdded(page.page, point);
}

void ProTerGen::ST::Cache::Clear()
{
	mLru.Resize(0);
	mLru.Resize((size_t)mDesc->AtlasNumTiles());

	mLoading.clear();
	mCurrent = 0;
}

void ProTerGen::ST::PhysicalTexture::Init
(
	Microsoft::WRL::ComPtr<ID3D12Device> device,
	DescriptorHeaps& descriptorHeaps,
	Texture* texture,
	STDesc* desc,
	SplitTextureInfo* info
)
{
	mTexture = texture;
	mDesc = desc;
	mInfo = info;
	{
		const uint32_t width = mDesc->AtlasNumTiles() * mDesc->TileWidth();
		const uint32_t height = width;
		const CD3DX12_HEAP_PROPERTIES texHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		const CD3DX12_RESOURCE_DESC texResDesc = CD3DX12_RESOURCE_DESC::Tex2D
		(
			mInfo->format,
			(size_t)width,
			height,
			1,
			1
		);
		ThrowIfFailed(device->CreateCommittedResource
		(
			&texHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&texResDesc,
			D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
			nullptr,
			IID_PPV_ARGS(mTexture->Resource.ReleaseAndGetAddressOf())
		));
		mTexture->Resource->SetName((L"Atlas_Texture_" + mTexture->Path).c_str());

		const size_t updBufferSize = Align((size_t)width * mInfo->TexelBytes(), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) * height;
		const CD3DX12_HEAP_PROPERTIES updHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		const CD3DX12_RESOURCE_DESC updResDesc = CD3DX12_RESOURCE_DESC::Buffer(updBufferSize);
		ThrowIfFailed(device->CreateCommittedResource
		(
			&updHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&updResDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(mTexture->UploadHeap.ReleaseAndGetAddressOf())
		));
		mTexture->UploadHeap->SetName((L"Atlas_Texture_Upload_" + mTexture->Path).c_str());
	}
	{
		const int32_t locationIndex = mTexture->Location;
		CD3DX12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeaps.GetSrvHeap().GetCPUHandle((uint32_t)locationIndex);
		const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc =
		{
		   .Format = mTexture->Resource->GetDesc().Format,
		   .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
		   .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		   .Texture2D =
		   {
			  .MostDetailedMip = 0,
			  .MipLevels = mTexture->Resource->GetDesc().MipLevels,
			  .ResourceMinLODClamp = 0.0f
		   },
		};
		device->CreateShaderResourceView(mTexture->Resource.Get(), &srvDesc, handle);
	}
}

void ProTerGen::ST::PhysicalTexture::Update(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList)
{
	if (mIsDirty)
	{
		CD3DX12_RESOURCE_BARRIER barriers[1] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(mTexture->Resource.Get(), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST),
		};
		cmdList->ResourceBarrier(_countof(barriers), barriers);
		CD3DX12_TEXTURE_COPY_LOCATION dst = CD3DX12_TEXTURE_COPY_LOCATION(mTexture->Resource.Get());
		const CD3DX12_TEXTURE_COPY_LOCATION src = CD3DX12_TEXTURE_COPY_LOCATION(mTexture->UploadHeap.Get());
		const CD3DX12_BOX srcBox = CD3DX12_BOX();
		// NOTE: Maybe execution complains about this.
		cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, &srcBox);
		barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(mTexture->Resource.Get(), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
		cmdList->ResourceBarrier(_countof(barriers), barriers);
		mIsDirty = false;
	}
}

void ProTerGen::ST::PhysicalTexture::Write(const void*& data, const Point& point)
{
	mIsDirty = true;
	const uint32_t totalTexWidth = (uint32_t)Align((size_t)mDesc->AtlasWidth() * (size_t)mInfo->TexelBytes(), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
	const size_t tileWidthBytes = (size_t)mDesc->TileWidth() * mInfo->TexelBytes();
	const size_t yoffset = (size_t)totalTexWidth * point.Y;
	const size_t xoffset = (size_t)point.X * tileWidthBytes;
	void* writePtr = nullptr;
	ThrowIfFailed(mTexture->UploadHeap->Map(0, nullptr, &writePtr));
	for (uint32_t y = 0; y < mDesc->TileWidth(); ++y)
	{
		void* dst = ((char*)writePtr + yoffset + y * totalTexWidth + xoffset);
		memcpy(dst, data, tileWidthBytes);
	}
	mTexture->UploadHeap->Unmap(0, nullptr);
	writePtr = nullptr;
}

void ProTerGen::ST::PhysicalTexture::Clear
(
	Microsoft::WRL::ComPtr<ID3D12Device> device,
	DescriptorHeaps& descriptorHeaps
)
{
	const uint32_t width = mDesc->AtlasNumTiles() * mDesc->TileWidth();
	const uint32_t height = width;
	const CD3DX12_HEAP_PROPERTIES texHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	const CD3DX12_RESOURCE_DESC texResDesc = CD3DX12_RESOURCE_DESC::Tex2D
	(
		mInfo->format,
		(size_t)width,
		height,
		1,
		1
	);
	ThrowIfFailed(device->CreateCommittedResource
	(
		&texHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&texResDesc,
		D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(mTexture->Resource.ReleaseAndGetAddressOf())
	));
	mTexture->Resource->SetName((L"Atlas_Texture_" + mTexture->Path).c_str());

	const size_t updBufferSize = Align((size_t)width * mInfo->TexelBytes(), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) * height;
	const CD3DX12_HEAP_PROPERTIES updHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	const CD3DX12_RESOURCE_DESC updResDesc = CD3DX12_RESOURCE_DESC::Buffer(updBufferSize);
	ThrowIfFailed(device->CreateCommittedResource
	(
		&updHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&updResDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mTexture->UploadHeap.ReleaseAndGetAddressOf())
	));
	mTexture->UploadHeap->SetName((L"Atlas_Texture_Upload_" + mTexture->Path).c_str());
	const int32_t locationIndex = mTexture->Location;
	CD3DX12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeaps.GetSrvHeap().GetCPUHandle((uint32_t)locationIndex);
	const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc =
	{
	   .Format = mTexture->Resource->GetDesc().Format,
	   .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
	   .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
	   .Texture2D =
	   {
		  .MostDetailedMip = 0,
		  .MipLevels = mTexture->Resource->GetDesc().MipLevels,
		  .ResourceMinLODClamp = 0.0f
	   },
	};
	device->CreateShaderResourceView(mTexture->Resource.Get(), &srvDesc, handle);
}

void ProTerGen::ST::IndirectionTexture::Init
(
	Microsoft::WRL::ComPtr<ID3D12Device> device,
	DescriptorHeaps& descriptorHeaps,
	Texture* texture,
	STDesc* desc
)
{
	mTexture = texture;
	mDesc = desc;
	{
		const uint32_t BYTES_PER_TEXEL = 2;
		const DXGI_FORMAT INDIRECTION_TEXTURE_FORMAT = DXGI_FORMAT_R8G8_UNORM;
		const uint32_t width = mDesc->NumTiles();
		const uint32_t height = width;
		const CD3DX12_HEAP_PROPERTIES texHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		const CD3DX12_RESOURCE_DESC texResDesc = CD3DX12_RESOURCE_DESC::Tex2D
		(
			INDIRECTION_TEXTURE_FORMAT,
			(size_t)width,
			height,
			1,
			1
		);
		ThrowIfFailed(device->CreateCommittedResource
		(
			&texHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&texResDesc,
			D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
			nullptr,
			IID_PPV_ARGS(mTexture->Resource.ReleaseAndGetAddressOf())
		));
		mTexture->Resource->SetName(L"Indirection_Texture_SplitTexture");

		const size_t updBufferSize = Align((size_t)width * BYTES_PER_TEXEL, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) * height;
		const CD3DX12_HEAP_PROPERTIES updHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		const CD3DX12_RESOURCE_DESC updResDesc = CD3DX12_RESOURCE_DESC::Buffer(updBufferSize);
		ThrowIfFailed(device->CreateCommittedResource
		(
			&updHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&updResDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(mTexture->UploadHeap.ReleaseAndGetAddressOf())
		));
		mTexture->UploadHeap->SetName(L"Indirection_Texture_SplitTexture_Upload");
		const int32_t locationIndex = mTexture->Location;
		CD3DX12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeaps.GetSrvHeap().GetCPUHandle((uint32_t)locationIndex);
		const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc =
		{
		   .Format = mTexture->Resource->GetDesc().Format,
		   .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
		   .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		   .Texture2D =
		   {
			  .MostDetailedMip     = 0,
			  .MipLevels           = mTexture->Resource->GetDesc().MipLevels,
			  .ResourceMinLODClamp = 0.0f
		   },
		};
		device->CreateShaderResourceView(mTexture->Resource.Get(), &srvDesc, handle);
	}
}

void ProTerGen::ST::IndirectionTexture::Update(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList)
{
	if (mIsDirty)
	{
		CD3DX12_RESOURCE_BARRIER barriers[1] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(mTexture->Resource.Get(), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST),
		};
		cmdList->ResourceBarrier(_countof(barriers), barriers);
		CD3DX12_TEXTURE_COPY_LOCATION dst = CD3DX12_TEXTURE_COPY_LOCATION(mTexture->Resource.Get());
		const CD3DX12_TEXTURE_COPY_LOCATION src = CD3DX12_TEXTURE_COPY_LOCATION(mTexture->UploadHeap.Get());
		const CD3DX12_BOX srcBox = CD3DX12_BOX();
		// NOTE: Maybe execution complains about this.
		cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, &srcBox);
		barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(mTexture->Resource.Get(), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
		cmdList->ResourceBarrier(_countof(barriers), barriers);
		mIsDirty = false;
	}
}

void ProTerGen::ST::IndirectionTexture::Clear(Microsoft::WRL::ComPtr<ID3D12Device> device, DescriptorHeaps& descriptorHeaps)
{
	const uint32_t BYTES_PER_TEXEL = 2;
	const DXGI_FORMAT INDIRECTION_TEXTURE_FORMAT = DXGI_FORMAT_R8G8_UNORM;
	const uint32_t width = mDesc->NumTiles();
	const uint32_t height = width;
	const CD3DX12_HEAP_PROPERTIES texHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	const CD3DX12_RESOURCE_DESC texResDesc = CD3DX12_RESOURCE_DESC::Tex2D
	(
		INDIRECTION_TEXTURE_FORMAT,
		(size_t)width,
		height,
		1,
		1
	);
	ThrowIfFailed(device->CreateCommittedResource
	(
		&texHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&texResDesc,
		D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(mTexture->Resource.ReleaseAndGetAddressOf())
	));
	mTexture->Resource->SetName(L"Indirection_Texture_SplitTexture");

	const size_t updBufferSize = Align((size_t)width * BYTES_PER_TEXEL, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) * height;
	const CD3DX12_HEAP_PROPERTIES updHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	const CD3DX12_RESOURCE_DESC updResDesc = CD3DX12_RESOURCE_DESC::Buffer(updBufferSize);
	ThrowIfFailed(device->CreateCommittedResource
	(
		&updHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&updResDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mTexture->UploadHeap.ReleaseAndGetAddressOf())
	));
	mTexture->UploadHeap->SetName(L"Indirection_Texture_SplitTexture_Upload");
	const int32_t locationIndex = mTexture->Location;
	CD3DX12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeaps.GetSrvHeap().GetCPUHandle((uint32_t)locationIndex);
	const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc =
	{
	   .Format = mTexture->Resource->GetDesc().Format,
	   .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
	   .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
	   .Texture2D =
	   {
		  .MostDetailedMip     = 0,
		  .MipLevels           = mTexture->Resource->GetDesc().MipLevels,
		  .ResourceMinLODClamp = 0.0f
	   },
	};
	device->CreateShaderResourceView(mTexture->Resource.Get(), &srvDesc, handle);
}

void ProTerGen::ST::IndirectionTexture::AddPage(const Page& page)
{
	mIsDirty = true;
	const size_t BYTES_PER_TEXEL = 2;
	void* writePtr = nullptr;
	ThrowIfFailed(mTexture->UploadHeap->Map(0, nullptr, &writePtr));
	const size_t offset = page.x + page.y * Align((size_t)mDesc->TilesPerSide() * BYTES_PER_TEXEL, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
	((unsigned char*)writePtr)[offset + 0] = (unsigned char)page.x;
	((unsigned char*)writePtr)[offset + 1] = (unsigned char)page.y;
	mTexture->UploadHeap->Unmap(0, nullptr);
}

void ProTerGen::ST::SplitTexture::Init
(
	Microsoft::WRL::ComPtr<ID3D12Device> device, 
	STDesc& desc, 
	std::vector<SplitTextureInfo>& textureInfo, 
	Materials& materials, 
	DescriptorHeaps& descriptorHeaps
)
{
	mDesc = std::move(desc);
	mTexturesInfo = std::move(textureInfo);
	mProcesingUnits.resize(mTexturesInfo.size());
	for (uint32_t i = 0; i < mTexturesInfo.size(); ++i)
	{
		Texture* t = materials.CreateEmptyTexture("SplitTexture_Atlas_" + std::to_string(i));
		mProcesingUnits[i] =
		{
			.Worker = std::make_unique<AsyncWorker>(),
			.Loader = std::make_unique<TextureLoader>(),
			.PhysicalTexture = std::make_unique<PhysicalTexture>(),
		};
		TextureProcessingUnit& tpu = mProcesingUnits[i];
		assert(false && "This is not ready to run.");
		tpu.Worker->Init();
		tpu.Loader->Init(&mDesc, &mTexturesInfo[i]);
		tpu.PhysicalTexture->Init(device, descriptorHeaps, t, &mDesc, &mTexturesInfo[i]);
	}
	mIndirectionTexture = std::make_unique<IndirectionTexture>();
	Texture* t = materials.CreateEmptyTexture("SplitTexture_Indirection");
	mIndirectionTexture->Init(device, descriptorHeaps, t, &mDesc);
}

void ProTerGen::ST::AsyncWorker::Init()
{
	throw - 1;
}

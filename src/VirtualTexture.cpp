#include "VirtualTexture.h"

#include <cmath>
#include <algorithm>
#include "MathHelpers.h"

ProTerGen::VT::Quadtree::Quadtree(Rectangle rect, uint32_t level) : mRect(rect), mLevel(level)
{
}

void ProTerGen::VT::Quadtree::Add(const Page& request, const Point& mapping)
{
	uint32_t scale = 1 << request.Mip;
	const Point p = {
		.X = request.X * scale,
		.Y = request.Y * scale
	};

	Quadtree* currentNode = this;
	while (request.Mip < currentNode->mLevel)
	{
		for (uint32_t i = 0; i < ChildPos::COUNT; ++i)
		{
			Rectangle r = GetSubRectangle(currentNode, (ChildPos)i);
			if (r.Contains(p))
			{
				if (!currentNode->mChild[i])
				{
					currentNode->mChild[i] = std::make_unique<Quadtree>(r, currentNode->mLevel - 1);
					currentNode = currentNode->mChild[i].get();
					break;
				}
				currentNode = currentNode->mChild[i].get();
				break;
			}

		}
	}

	currentNode->mMapping = mapping;
}

void ProTerGen::VT::Quadtree::Remove(Page request)
{
	ChildPos index = ChildPos::_NULL;
	Quadtree* node = FindPage(request, index);

	if (node)
	{
		node->mChild[index].reset();
	}
}

void ProTerGen::VT::Quadtree::Write(Fillable& image, uint32_t mipLevel) const
{
	if (mLevel >= mipLevel)
	{
		const Rectangle r =
		{
			.X = mRect.X >> mipLevel,
			.Y = mRect.Y >> mipLevel,
			.Width = mRect.Width >> mipLevel,
			.Height = mRect.Height >> mipLevel
		};

		byte data[4] = { (byte)mMapping.X, (byte)mMapping.Y, (byte)mLevel, 255 };
		image.FillRegion(r, data);

		for(auto & child : mChild)
		{
			if (child)
			{
				child->Write(image, mipLevel);
			}
		}
	}
}

ProTerGen::Rectangle ProTerGen::VT::Quadtree::GetSubRectangle(Quadtree* node, ChildPos index) const
{
	const uint32_t hw = node->mRect.Width / 2;
	const uint32_t hh = node->mRect.Height / 2;
	switch (index)
	{
	case ChildPos::NW:
		return Rectangle{ .X = node->mRect.X     , .Y = node->mRect.Y     , .Width = hw, .Height = hh };
	case ChildPos::NE:
		return Rectangle{ .X = node->mRect.X + hw, .Y = node->mRect.Y     , .Width = hw, .Height = hh };
	case ChildPos::SW:
		return Rectangle{ .X = node->mRect.X     , .Y = node->mRect.Y + hh, .Width = hw, .Height = hh };
	case ChildPos::SE:
		return Rectangle{ .X = node->mRect.X + hw, .Y = node->mRect.Y + hh, .Width = hw, .Height = hh };
	case ChildPos::_NULL:
	case ChildPos::COUNT:
	default:
		assert(false && "Invalid child pos index. This should never happen");
		throw - 1;
	}

}

ProTerGen::VT::Quadtree* ProTerGen::VT::Quadtree::FindPage(const Page& request, ChildPos& index) 
{
	Quadtree* node = this;

	const uint32_t scale = 1 << request.Mip;
	const Point p = { .X = request.X * scale, .Y = request.Y * scale };

	bool find = false;
	while (!find)
	{
		find = true;

		for (uint32_t i = 0; i < ChildPos::COUNT; ++i)
		{
			if (node->mChild[i] != nullptr && node->mChild[i]->mRect.Contains(p))
			{
				if (request.Mip == node->mLevel - 1)
				{
					index = (ChildPos)i;
					return node;
				}

				else
				{
					node = node->mChild[i].get();
					find = false;
				}
			}
		}
	}

	index = ChildPos::_NULL;
	return nullptr;
}

ProTerGen::VT::StagingBufferPool::~StagingBufferPool()
{
}

void ProTerGen::VT::StagingBufferPool::Init
(
	Microsoft::WRL::ComPtr<ID3D12Device> device,
	uint32_t width,
	uint32_t height,
	uint32_t sizePerTexel,
	uint32_t count,
	Access access
)
{
	mWidth = width;
	mHeight = height;
	mFormatSize = sizePerTexel;
	mAccess = access;

	const size_t size = (size_t)mWidth * mHeight * mFormatSize;

	const D3D12_HEAP_TYPE heapType = mAccess == WRITE_TO_GPU ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_READBACK;
	const D3D12_RESOURCE_STATES initialResState = mAccess == WRITE_TO_GPU ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COPY_DEST;
	const CD3DX12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(heapType);
	const CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
	mResources.resize(count);
	for (uint32_t i = 0; i < count; ++i)
	{
		ThrowIfFailed(device->CreateCommittedResource
		(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			initialResState,
			nullptr,
			IID_PPV_ARGS(mResources[i].ReleaseAndGetAddressOf())
		));

		const std::wstring debugName = L"Staging_BufferPool_Res_" + std::to_wstring(i);
		mResources[i]->SetName(debugName.c_str());
	}
}

void ProTerGen::VT::StagingBufferPool::Next()
{
	mIndex = (++mIndex) % (mResources.size());
}

void ProTerGen::VT::StagingBufferPool::WriteFrom(const data_ptr src, const Rectangle& dstRegion)
{
	assert(mAccess == WRITE_TO_GPU && "Only perform writing operations.");

	const Rectangle srcRegion = { .X = 0, .Y = 0, .Width = dstRegion.Width, .Height = dstRegion.Height };
	const Point dstOffset = { .X = dstRegion.X, .Y = dstRegion.Y };
	WriteFrom(src, srcRegion, dstOffset);
}

void ProTerGen::VT::StagingBufferPool::WriteFrom(const data_ptr src, const Rectangle& srcRegion, const Point& dstOffset)
{
	assert(mAccess == WRITE_TO_GPU && "Only perform writing operations.");

	Microsoft::WRL::ComPtr<ID3D12Resource> res = mResources[mIndex];
	data_ptr data = nullptr;
	ThrowIfFailed(res->Map(0, nullptr, &data));

	const uint32_t width = min(srcRegion.Width, mWidth - dstOffset.X);
	const uint32_t height = min(srcRegion.Height, mHeight - dstOffset.Y);

	const size_t size = (size_t)width * mFormatSize;

	for (uint32_t y = 0; y < srcRegion.Height; ++y)
	{
		const size_t srcX = (size_t)srcRegion.X * mFormatSize;
		const size_t srcY = (size_t)(y + srcRegion.Y) * width * mFormatSize;

		const size_t dstX = (size_t)dstOffset.X * mFormatSize;
		const size_t dstY = (size_t)(y + dstOffset.Y) * width * mFormatSize;

		const data_ptr pSrc = ((const byte_ptr)src + srcX + srcY);
		data_ptr pDst = (byte_ptr)data + dstX + dstY;

		memcpy(pDst, pSrc, size);
	}

	res->Unmap(0, nullptr);
	data = nullptr;
}

void ProTerGen::VT::StagingBufferPool::CopyTo
(
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
	Microsoft::WRL::ComPtr<ID3D12Resource> dstTexture,
	const Rectangle& srcRegion,
	const Point& dstOffset
) const
{
	assert(mAccess == WRITE_TO_GPU && "Only perform writing operations.");

	const D3D12_SUBRESOURCE_FOOTPRINT pitchedDesc =
	{
		.Format   = dstTexture->GetDesc().Format,
		.Width    = mWidth,
		.Height   = mHeight,
		.Depth    = 1,
		.RowPitch = (uint32_t)Align(mWidth * mFormatSize, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1)
	};
	uint32_t other = (uint32_t)Align(srcRegion.Width * mFormatSize, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) / mFormatSize;

	const D3D12_PLACED_SUBRESOURCE_FOOTPRINT placedTexture2D =
	{
		.Offset    = 0,
		.Footprint = pitchedDesc
	};

	CD3DX12_TEXTURE_COPY_LOCATION dst = CD3DX12_TEXTURE_COPY_LOCATION(dstTexture.Get(), 0);
	const CD3DX12_TEXTURE_COPY_LOCATION src = CD3DX12_TEXTURE_COPY_LOCATION(mResources[mIndex].Get(), placedTexture2D);
	const D3D12_BOX b =
	{
		.left   = srcRegion.X,
		.top    = srcRegion.Y,
		.front  = 0,
		.right  = srcRegion.Width,
		.bottom = srcRegion.Height,
		.back   = 1
	};
	commandList->CopyTextureRegion(
		&dst,
		dstOffset.X, dstOffset.Y, 0,
		&src,
		&b);
}

void ProTerGen::VT::StagingBufferPool::CopyFrom
(
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, 
	Microsoft::WRL::ComPtr<ID3D12Resource> srcTexture, 
	const Rectangle& srcRegion, 
	const Point& dstOffset
)
{
	assert(mAccess == READ_FROM_GPU && "Only perform reading operations.");

	const D3D12_SUBRESOURCE_FOOTPRINT pitchedDesc =
	{
		.Format   = srcTexture->GetDesc().Format,
		.Width    = mWidth,
		.Height   = mHeight,
		.Depth    = 1,
		.RowPitch = (uint32_t)Align(mWidth * mFormatSize, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1)
	};

	const D3D12_PLACED_SUBRESOURCE_FOOTPRINT placedTexture2D =
	{
		.Offset    = 0,
		.Footprint = pitchedDesc
	};

	CD3DX12_TEXTURE_COPY_LOCATION dst = CD3DX12_TEXTURE_COPY_LOCATION(mResources[mIndex].Get(), placedTexture2D);
	const CD3DX12_TEXTURE_COPY_LOCATION src = CD3DX12_TEXTURE_COPY_LOCATION(srcTexture.Get(), 0);
	const D3D12_BOX b =
	{
		.left   = srcRegion.X,
		.top    = srcRegion.Y,
		.front  = 0,
		.right  = srcRegion.Width,
		.bottom = srcRegion.Height,
		.back   = 1
	};
	commandList->CopyTextureRegion(
		&dst,
		dstOffset.X, dstOffset.Y, 0,
		&src,
		&b);	
}

void ProTerGen::VT::StagingBufferPool::CopyFrom
(
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, 
	Microsoft::WRL::ComPtr<ID3D12Resource> srcTexture
)
{
	const Point p = { .X = 0, .Y = 0 };
	const Rectangle r = { .X = 0, .Y = 0, .Width = mWidth, .Height = mHeight };
	CopyFrom(commandList, srcTexture, r, p);
}

void ProTerGen::VT::StagingBufferPool::WriteTo(data_ptr dst)
{
	assert(mAccess == READ_FROM_GPU && "Only perform reading operations.");

	Microsoft::WRL::ComPtr<ID3D12Resource>& res = mResources[mIndex];

	data_ptr map = nullptr;
	D3D12_RANGE range = { .Begin = 0, .End = (size_t)mWidth * mHeight * mFormatSize };
	ThrowIfFailed(res->Map(0, &range, &map));
	
	memcpy(dst, map, (size_t)mWidth * mHeight * mFormatSize);
		 
	res->Unmap(0, nullptr);
}

void ProTerGen::VT::FeedbackBuffer::Init
(
	Microsoft::WRL::ComPtr<ID3D12Device> device,
	DescriptorHeaps& descriptorHeaps,
	const VTDesc* info, 
	size_t size
)
{
	mInfo = info;
	mSize = size;

	mIndexer.Init(*info);

	{
		// Create a new resource for RT and DS.

		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
		CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D
		(
			DXGI_FORMAT_R32G32B32A32_FLOAT,
			mSize,
			(uint32_t)mSize,
			1,
			1,
			1,
			0,
			D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
		);
		D3D12_CLEAR_VALUE clearColor = { .Format = DXGI_FORMAT_R32G32B32A32_FLOAT, .Color = { 0.0f, 0.0f, 0.0f, 0.0f } };
		ThrowIfFailed(device->CreateCommittedResource
		(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			&clearColor,
			IID_PPV_ARGS(mRenderTargetBuffer.ReleaseAndGetAddressOf())
		));

		desc = CD3DX12_RESOURCE_DESC::Tex2D
		(
			DXGI_FORMAT_D32_FLOAT,
			mSize,
			(uint32_t)mSize,
			1,
			1,
			1,
			0,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
		);
		D3D12_CLEAR_VALUE optClear =
		{
			.Format = DXGI_FORMAT_D32_FLOAT,
			.DepthStencil = {
				.Depth = 1.0f,
				.Stencil = 0
			}
		};
		ThrowIfFailed(device->CreateCommittedResource
		(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&optClear,
			IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())
		));
	}

	{
		// Save RT and DS reference in the descriptor heap

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc =
		{
			.Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
			.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
			.Texture2D =
			{
				.MipSlice = 0,
				.PlaneSlice = 0
			}
		};
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc =
		{
			.Format = DXGI_FORMAT_D32_FLOAT,
			.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
			.Flags = D3D12_DSV_FLAG_NONE,
			.Texture2D =
			{
				.MipSlice = 0,

			},
		};

		const std::string rtName = "FB_RTV";
		const std::string dsName = "FB_DSV";
		const uint32_t rtIdx = descriptorHeaps.SetRtvIndex(rtName);
		const uint32_t dsIdx = descriptorHeaps.SetDsvIndex(dsName);

		device->CreateRenderTargetView(mRenderTargetBuffer.Get(), &rtvDesc, descriptorHeaps.GetRtvHeap().GetCPUHandle(rtIdx));
		device->CreateDepthStencilView(mDepthStencilBuffer.Get(), &dsvDesc, descriptorHeaps.GetDsvHeap().GetCPUHandle(dsIdx));

		mRenderTargetBuffer->SetName(L"VT_FeedbackBufferRT");
		mDepthStencilBuffer->SetName(L"VT_FeedbackBufferDS");
	}

	mResources.Init(device, (uint32_t)mSize, (uint32_t)mSize, sizeof(float) * 4, 1, StagingBufferPool::Access::READ_FROM_GPU);

	mViewport =
	{
		.TopLeftX = 0.0f,
		.TopLeftY = 0.0f,
		.Width = static_cast<float>(mSize),
		.Height = static_cast<float>(mSize),
		.MinDepth = 0.0f,
		.MaxDepth = 0.0f
	};
}

void ProTerGen::VT::FeedbackBuffer::Clear
(
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
	const DescriptorHeaps& descriptorHeaps
)
{
	const std::string rtName = "FB_RTV";
	const std::string dsName = "FB_DSV";
	const uint32_t rtIdx = descriptorHeaps.GetIndex(rtName);
	const uint32_t dsIdx = descriptorHeaps.GetIndex(dsName);

	const CD3DX12_CPU_DESCRIPTOR_HANDLE rtHandle = descriptorHeaps.GetRtvHeap().GetCPUHandle(rtIdx);
	const CD3DX12_CPU_DESCRIPTOR_HANDLE dsHandle = descriptorHeaps.GetDsvHeap().GetCPUHandle(dsIdx);

	const float color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	commandList->ClearRenderTargetView(rtHandle, color, 0, nullptr);
	commandList->ClearDepthStencilView(dsHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	mRequests.clear();
}

void ProTerGen::VT::FeedbackBuffer::SetAsReadable(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList)
{
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition
	(
		mRenderTargetBuffer.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_COPY_SOURCE
	);
	commandList->ResourceBarrier(1, &barrier);
}

void ProTerGen::VT::FeedbackBuffer::SetAsWriteable(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList)
{
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition
	(
		mRenderTargetBuffer.Get(),
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	);
	commandList->ResourceBarrier(1, &barrier);
}

void ProTerGen::VT::FeedbackBuffer::SetAsRenderTarget
(
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
	const DescriptorHeaps& descriptorHeaps
)
{
	const std::string rtName = "FB_RTV";
	const std::string dsName = "FB_DSV";
	const uint32_t rtIdx = descriptorHeaps.GetIndex(rtName);
	const uint32_t dsIdx = descriptorHeaps.GetIndex(dsName);

	const CD3DX12_CPU_DESCRIPTOR_HANDLE rtHandle = descriptorHeaps.GetRtvHeap().GetCPUHandle(rtIdx);
	const CD3DX12_CPU_DESCRIPTOR_HANDLE dsHandle = descriptorHeaps.GetDsvHeap().GetCPUHandle(dsIdx);

	commandList->RSSetViewports(1, &mViewport);
	commandList->OMSetRenderTargets(1, &rtHandle, true, &dsHandle);
}

void ProTerGen::VT::FeedbackBuffer::Copy(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList)
{
	//SetAsReadable(commandList);
	mResources.CopyFrom(commandList, mRenderTargetBuffer);
}

void ProTerGen::VT::FeedbackBuffer::Download()
{	
	_Color color = { 0.0f, 0.0f, 0.0f, 0.0f };
	const uint32_t dataSize = sizeof(color);
	data_ptr data = malloc(mSize * mSize * dataSize);
	if (!data)
	{
		assert(data != nullptr && "Error in memory buffer creation for downloading feddback buffer.");
		return;
	}
	mResources.WriteTo(data);

	for (size_t i = 0; i < (size_t)mSize * mSize; ++i)
	{
		color = ((_Color*)data)[i];
		if (color.w >= 0.99f)
		{
			Page request = { .X = (uint32_t)color.x, .Y = (uint32_t)color.y, .Mip = (uint32_t)color.z };
			AddRequest(request);
		}
	}

	free(data);
}

void ProTerGen::VT::FeedbackBuffer::AddRequest(Page request)
{
	const uint32_t pageTableSize = mInfo->VTTilesPerRowExp;
	const uint32_t count = pageTableSize - request.Mip + 1;

	for (uint32_t i = 0; i < count; ++i)
	{
		const uint32_t xpos = request.X >> i;
		const uint32_t ypos = request.Y >> i;
		Page page = { .X = xpos, .Y = ypos, .Mip = request.Mip + i };

		if (!mIndexer.IsValid(page))
		{
			return;
		}

		++mRequests[mIndexer.PageIndex(page)];
	}
}

void ProTerGen::VT::PageTableIndirection::Init(Microsoft::WRL::ComPtr<ID3D12Device> device, const VTDesc* info, Texture* const& texture)
{
	mInfo = info;
	mTexture = texture;

	const size_t pageTableSize = mInfo->VTTilesPerRow();

	const Rectangle r = { .X = 0, .Y = 0, .Width = (uint32_t)pageTableSize, .Height = (uint32_t)pageTableSize };
	const uint32_t level = mInfo->VTTilesPerRowExp;
	mQuadTree = std::make_unique<VT::Quadtree>(r, level);

	// Init texture
	const DXGI_FORMAT format = VT_INDIRECTION_FORMAT;
	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D
	(
		format,
		pageTableSize,
		(uint32_t)pageTableSize
	);
	ThrowIfFailed(device->CreateCommittedResource
	(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(mTexture->Resource.ReleaseAndGetAddressOf())
	));

	const uint32_t mips = mTexture->Resource->GetDesc().MipLevels;
	const uint32_t pageTableSizeLog2 = Log2(pageTableSize);
	assert(mips == (pageTableSizeLog2 + 1));
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT* fp = (D3D12_PLACED_SUBRESOURCE_FOOTPRINT*)malloc((size_t)sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) * mips);
	if (!fp) { throw - 1; }
	size_t uploadBufferSize = 0;
	device->GetCopyableFootprints(&desc, 0, mips, 0, fp, nullptr, nullptr, &uploadBufferSize);

	heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	desc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
	ThrowIfFailed(device->CreateCommittedResource
	(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mTexture->UploadHeap.ReleaseAndGetAddressOf())
	));

	mTexture->Resource->SetName(L"Indirection_Texture");
	mTexture->UploadHeap->SetName(L"Indirection_Texture_Upload");

	// Init raw images
	mRawData.resize((size_t)mips);

	data_ptr data_p = nullptr;
	ThrowIfFailed(mTexture->UploadHeap->Map(0, nullptr, &data_p));
	if (!data_p)
	{
		assert(data_p && "Error mapping upload heap data");
		return;
	}
	for (uint32_t i = 0; i < mips; ++i)
	{
		const uint32_t size = mInfo->VTTilesPerRow() >> i;

		mRawData[i].Init((byte_ptr)data_p + fp[i].Offset, fp[i], VT_INDIRECTION_BYTES_PER_PIXEL);
	}

	free(fp);
}

void ProTerGen::VT::PageTableIndirection::Update(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList)
{
	const uint32_t pageTableSizeLog2 = mInfo->VTTilesPerRowExp;

	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition
	(
		mTexture->Resource.Get(),
		D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_COPY_DEST
	);
	commandList->ResourceBarrier(1, &barrier);

	for (uint32_t i = 0; i < pageTableSizeLog2 + 1; ++i)
	{
		mQuadTree->Write(mRawData[i], i);

		CD3DX12_TEXTURE_COPY_LOCATION dst = CD3DX12_TEXTURE_COPY_LOCATION(mTexture->Resource.Get(), i);
		const CD3DX12_TEXTURE_COPY_LOCATION src = CD3DX12_TEXTURE_COPY_LOCATION(mTexture->UploadHeap.Get(), mRawData[i].Footprint);
		const D3D12_BOX b =
		{
			.left = 0,
			.top = 0,
			.front = 0,
			.right = mRawData[i].Footprint.Footprint.Width,
			.bottom = mRawData[i].Footprint.Footprint.Height,
			.back = 1
		};
		
		commandList->CopyTextureRegion(
			&dst,
			0, 0, 0,
			&src,
			&b);		
	}

	barrier = CD3DX12_RESOURCE_BARRIER::Transition
	(
		mTexture->Resource.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
	);
	commandList->ResourceBarrier(1, &barrier);
}

void ProTerGen::VT::PageTableIndirection::AddPage(const Page& request, const Point& mapping)
{
	mQuadTree->Add(request, mapping);
}

void ProTerGen::VT::PageTableIndirection::RemovePage(const Page& page)
{
	mQuadTree->Remove(page);
}

void ProTerGen::VT::PageTableIndirection::Clear(Microsoft::WRL::ComPtr<ID3D12Device> device, DescriptorHeaps& descriptorHeaps)
{
	const int32_t locationIndex = mTexture->Location;
	const size_t pageTableSize = mInfo->VTTilesPerRow();

	const Rectangle r = { .X = 0, .Y = 0, .Width = (uint32_t)pageTableSize, .Height = (uint32_t)pageTableSize };
	const uint32_t level = mInfo->VTTilesPerRowExp;
	mQuadTree.reset();
	mQuadTree = std::make_unique<VT::Quadtree>(r, level);

	const DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D
	(
		format,
		pageTableSize,
		(uint32_t)pageTableSize
	);
	ThrowIfFailed(device->CreateCommittedResource
	(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(mTexture->Resource.ReleaseAndGetAddressOf())
	));

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

	const uint32_t mips = mTexture->Resource->GetDesc().MipLevels;
	const uint32_t pageTableSizeLog2 = Log2(pageTableSize);
	assert(mips == (pageTableSizeLog2 + 1));
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT* fp = (D3D12_PLACED_SUBRESOURCE_FOOTPRINT*)malloc((size_t)sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) * mips);
	if (!fp) { throw - 1; }
	size_t uploadBufferSize = 0;
	device->GetCopyableFootprints(&desc, 0, mips, 0, fp, nullptr, nullptr, &uploadBufferSize);

	heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	desc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
	mTexture->UploadHeap->Unmap(0, nullptr);
	ThrowIfFailed(device->CreateCommittedResource
	(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mTexture->UploadHeap.ReleaseAndGetAddressOf())
	));

	mTexture->Resource->SetName(L"Indirection_Texture");
	mTexture->UploadHeap->SetName(L"Indirection_Texture_Upload");

	mRawData.clear();
	mRawData.resize((size_t)mips);

	data_ptr data_p = nullptr;
	ThrowIfFailed(mTexture->UploadHeap->Map(0, nullptr, &data_p));
	if (!data_p)
	{
		assert(data_p && "Error mapping upload heap data");
		return;
	}
	for (uint32_t i = 0; i < mips; ++i)
	{
		const uint32_t size = mInfo->VTTilesPerRow() >> i;

		mRawData[i].Init((byte_ptr)data_p + fp[i].Offset, fp[i], 4);
	}

	free(fp);	
}

void ProTerGen::VT::PageCache::Init(uint32_t count)
{
	mCount = count;

	mLru.Resize((size_t)mCount * mCount);
	mLru.OnRemove([&](Page page, Point point) { mOnRemove(page, point); });
}

bool ProTerGen::VT::PageCache::UpdatePagePosition(const Page& page)
{
	if (!mLoading.contains(page))
	{
		Point point = {};
		return mLru.TryGet(page, point, true);
	}
	return false;
}

bool ProTerGen::VT::PageCache::Request(Page page)
{
	if (!mLoading.contains(page))
	{
		Point point = {};
		if (!mLru.TryGet(page, point, false))
		{
			mLoading.insert(page);
			mSubmit(page);
			return true;
		}
	}
	return false;
}

void ProTerGen::VT::PageCache::LoadComplete(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, const MultiPage& page)
{
	mLoading.erase(page.page);
	Point point = { };

	if (mCurrent == mCount * mCount)
	{
		point = mLru.RemoveLast();
	}
	else
	{
		point = { .X = mCurrent % mCount, .Y = mCurrent / mCount };
		++mCurrent;
	}

	mUploadData(commandList, point, page.dataPtrs);
	mLru.Add(page.page, point);
	mOnPageAdded(page.page, point);
}

void ProTerGen::VT::PageCache::Clear()
{
	mLru.Resize(0);
	mLru.Resize((size_t)mCount * mCount);

	mLoading.clear();
	mCurrent = 0;
}

#pragma once

#include "VirtualTextureCommon.h"

#include <DirectXMath.h>
#include <array>
#include <functional>

#include "ConcurrentQueue.h"
#include "MathHelpers.h"
#include "ImageRawData.h"
#include "Texture.h"
#include "DescriptorHeaps.h"
#include "PointerHelper.h"
#include "LRUCache.h"
#include "PageLoaderGpuGen.h"

#if _DEBUG && PRINT_PERFORMANCE_TIMES
#include "Timer.h"
#endif

namespace ProTerGen
{
	namespace VT
	{		
		class Quadtree
		{
		private:
			enum ChildPos : int32_t
			{
				_NULL = -1,
				NW = 0,
				NE = 1,
				SW = 2,
				SE = 3,
				COUNT = 4
			};
		public:
			uint32_t mLevel;
			Rectangle mRect;

			Point mMapping;

			std::array<std::unique_ptr<Quadtree>, (size_t)Quadtree::ChildPos::COUNT> mChild = { nullptr, nullptr, nullptr, nullptr };

			explicit Quadtree(Rectangle rect, uint32_t level);

			void Add(const Page& request, const Point& mapping);
			void Remove(Page request);
			void Write(Fillable& image, uint32_t mipLevel) const;
		private:			
			Rectangle GetSubRectangle(Quadtree* node, ChildPos pos) const;
			Quadtree* FindPage(const Page& request, ChildPos& index);
		};

		class StagingBufferPool
		{
		public:
			enum Access
			{
				READ_FROM_GPU,
				WRITE_TO_GPU
			};

			~StagingBufferPool();

			void Init
			(
				Microsoft::WRL::ComPtr<ID3D12Device> device,
				uint32_t width,
				uint32_t height,
				uint32_t sizePerTexel,
				uint32_t count,
				Access access
			);

			void Next();

			void WriteFrom(const data_ptr src, const Rectangle& dstRegion);
			void WriteFrom(const data_ptr src, const Rectangle& srcRegion, const Point& dstOffset);
			void CopyTo
			(
				Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
				Microsoft::WRL::ComPtr<ID3D12Resource> dstTexture,
				const Rectangle& srcRegion,
				const Point& dstOffset
			) const;
			void CopyFrom
			(
				Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
				Microsoft::WRL::ComPtr<ID3D12Resource> srcTexture,
				const Rectangle& srcRegion,
				const Point& dstOffset
			);
			void CopyFrom
			(
				Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
				Microsoft::WRL::ComPtr<ID3D12Resource> srcTexture
			);
			void WriteTo(data_ptr dst);

		private:
			std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> mResources;
			uint32_t mIndex = 0;
			uint32_t mWidth = 0;
			uint32_t mHeight = 0;
			uint32_t mFormatSize = 0;
			Access mAccess = WRITE_TO_GPU;
		};

		// Physical Texture in GPU. Holds pages in GPU.
		template<size_t _Size>
		class TextureAtlas
		{
		public:
			TextureAtlas() {}
			~TextureAtlas() { Dispose(); }

			void Init
			(
				Microsoft::WRL::ComPtr<ID3D12Device> device,
				std::array<Texture*, _Size>& tex,
				const std::array<DXGI_FORMAT, _Size>& textureFormat,
				const std::array<uint32_t, _Size>& textureSizesPerTexel,
				const VTDesc* info,
				uint32_t auxiliarBufferCount,
				uint32_t nColumns,
				uint32_t nRows = 0
			) {
				mTextures = std::move(tex);
				mInfo = info;
				mColumns = nColumns;
				mRows = (nRows == 0 ? nColumns : nRows);

				CreateTexturesGpu(device, textureFormat);
				const uint32_t pageSize = mInfo->BorderedTileSize();
				for (uint32_t i = 0; i < _Size; ++i)
				{
					const uint32_t alignedPageSize = (uint32_t)Align(mInfo->BorderedTileSize() * textureSizesPerTexel[i], D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) / textureSizesPerTexel[i];
					mStagingBufferPool[i].Init(device, alignedPageSize, pageSize, textureSizesPerTexel[i], auxiliarBufferCount, StagingBufferPool::Access::WRITE_TO_GPU);
				}
			}

			void Dispose()
			{
				for (uint32_t i = 0; i < _Size; ++i)
				{
					Texture*& texture = mTextures[i];
					if (texture && texture->UploadHeap)
					{
						texture->UploadHeap->Unmap(0, nullptr);
					}
					if (texture)
					{
						texture->Resource.Reset();
						texture->UploadHeap.Reset();
					}
				}
			}

			void UploadPage(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, const Point& position, std::vector<data_ptr>& data)
			{
				CD3DX12_RESOURCE_BARRIER barriers[_Size] = {};
				for (uint32_t i = 0; i < _Size; ++i)
				{
					Texture*& texture = mTextures[i];
					if (texture && texture->Resource)
					{
						barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition
						(
							texture->Resource.Get(),
							D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
							D3D12_RESOURCE_STATE_COPY_DEST
						);
					}
				}
				commandList->ResourceBarrier(_Size, barriers);
				
				const Rectangle pageRect = { .X = 0, .Y = 0, .Width = mInfo->BorderedTileSize(), .Height = mInfo->BorderedTileSize() };
				const uint32_t alignedWidth = (uint32_t)Align(mInfo->BorderedTileSize() * _Size, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
				const Rectangle alignedPageRect = { .X = 0, .Y = 0, .Width = alignedWidth, .Height = mInfo->BorderedTileSize() };
				const Point sizeInPoint = { .X = position.X * mInfo->BorderedTileSize(), .Y = position.Y * mInfo->BorderedTileSize() };

				for (uint32_t i = 0; i < _Size; ++i)
				{
					Texture*& texture = mTextures[i];
					StagingBufferPool& bufferPool = mStagingBufferPool[i];
					bufferPool.WriteFrom(data[i], alignedPageRect);
					bufferPool.CopyTo(commandList, texture->Resource, pageRect, sizeInPoint);
					bufferPool.Next();

					barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition
					(
						texture->Resource.Get(),
						D3D12_RESOURCE_STATE_COPY_DEST,
						D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
					);
				}
				commandList->ResourceBarrier(_Size, barriers);
			}


			inline int32_t GetTexId(uint32_t id) const
			{ 
				assert(id < _Size);
				if (mTextures[(uint32_t)id])
				{
					return mTextures[(uint32_t)id]->Location;
				}
				return -1;
			}

			void Clear(Microsoft::WRL::ComPtr<ID3D12Device> device, DescriptorHeaps& descriptorHeaps, const std::array<DXGI_FORMAT, _Size>& textureFormat)
			{
				CreateTexturesGpu(device, textureFormat);
				for (uint32_t i = 0; i < _Size; ++i)
				{
					Texture*& texture = mTextures[i];
					const int32_t locationIndex = texture->Location;

					CD3DX12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeaps.GetSrvHeap().GetCPUHandle((uint32_t)locationIndex);
					const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc =
					{
					   .Format                  = texture->Resource->GetDesc().Format,
					   .ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D,
					   .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
					   .Texture2D =
					   {
						  .MostDetailedMip     = 0,
						  .MipLevels           = texture->Resource->GetDesc().MipLevels,
						  .ResourceMinLODClamp = 0.0f
					   },
					};
					device->CreateShaderResourceView(texture->Resource.Get(), &srvDesc, handle);
				}
			}

		private:
			void CreateTexturesGpu(Microsoft::WRL::ComPtr<ID3D12Device> device, const std::array<DXGI_FORMAT, _Size> textureFormats)
			{
				const uint32_t pageSize = mInfo->BorderedTileSize();
				
				const std::wstring textureName = L"Atlas_Texture_";
				const CD3DX12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
				CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D
				(
					DXGI_FORMAT_R32_TYPELESS,
					(size_t)mColumns * pageSize,
					mRows * pageSize,
					1,
					1
				);
				for (uint32_t i = 0; i < _Size; ++i)
				{
					resourceDesc.Format = textureFormats[i];
					ThrowIfFailed(device->CreateCommittedResource
					(
						&heapProps,
						D3D12_HEAP_FLAG_NONE,
						&resourceDesc,
						D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
						nullptr,
						IID_PPV_ARGS(mTextures[i]->Resource.ReleaseAndGetAddressOf())
					));
					mTextures[i]->Resource->SetName((textureName + std::to_wstring(i)).c_str());
				}
			}

		private:

			uint32_t mColumns = 0;
			uint32_t mRows = 0;

			const VTDesc* mInfo = nullptr;

			std::array<Texture*, _Size> mTextures = {};
			std::array<StagingBufferPool, _Size> mStagingBufferPool = {};
		};		

		// Manages texture atlas and tracks page loading
		class PageCache
		{
		private:
			using submit_func_t = std::function<void(const Page&)>;
			using upload_func_t = std::function<void(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>, const Point&, std::vector<void*>&)>;
			using add_func_t    = std::function<void(const Page&, const Point&)>;
			using remove_func_t = std::function<void(const Page&, const Point&)>;
		public:
			void OnPageRequestedToAdd(submit_func_t func) { mSubmit = func; }
			void OnPageDataComputedUpload(upload_func_t func) { mUploadData = func; }
			void OnPageAddedToCache(add_func_t func) { mOnPageAdded = func; }
			void OnPageRemovedFromCache(remove_func_t func) { mOnRemove = func; }

			void Init(uint32_t rowCount);
			bool UpdatePagePosition(const Page& page);
			bool Request(Page page);

			void Clear();
			void LoadComplete(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, const MultiPage&data);
		private:
			submit_func_t mSubmit = [](const Page&) {};
			upload_func_t mUploadData = [](Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>, const Point&, std::vector<void*>&) {};
			add_func_t mOnPageAdded = [](const Page&, const Point&) {};
			remove_func_t mOnRemove = [](const Page&, const Point&) {};

			uint32_t mCount = 0;
			uint32_t mCurrent = 0;

			LRUCache<Page, Point> mLru;
			std::unordered_set<Page> mLoading;
		};

		// Indirection texture
		class PageTableIndirection
		{
		public:
			void Init
			(
				Microsoft::WRL::ComPtr<ID3D12Device> device,
				const VTDesc* info,
				Texture* const & texture
			);

			void Update(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList);
			void AddPage(const Page& page, const Point& p);
			void RemovePage(const Page& page);

			inline int32_t GetTexId() const
			{
				if (mTexture)
				{
					return mTexture->Location;
				}
				return -1;
			}

			void Clear(Microsoft::WRL::ComPtr<ID3D12Device> device, DescriptorHeaps& descriptorHeaps);
		private:
			const VTDesc* mInfo = nullptr;

			Texture* mTexture = nullptr;
			std::vector<ImageRawDataFootprint> mRawData;

			std::unique_ptr<VT::Quadtree> mQuadTree;
		};

		// Intermediate first pass to address which textures will be painted.
		class FeedbackBuffer
		{
		public:
			static const uint32_t RTVCount = 1;
			static const uint32_t DSVCount = 1;

			void Init
			(
				Microsoft::WRL::ComPtr<ID3D12Device> device,
				DescriptorHeaps& descriptorHeaps,
				const VTDesc* info,
				size_t size
			);
			void Clear
			(
				Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
				const DescriptorHeaps& descriptorHeaps
			);
			void SetAsReadable(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList);
			void SetAsWriteable(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList);
			void SetAsRenderTarget(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, const DescriptorHeaps& descriptorHeaps);
			void Copy(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList);
			void Download();

			const std::unordered_map<uint32_t, uint32_t>& Requests() { return mRequests; }
		private:
			struct _Color { float x; float y; float z; float w; };

			void AddRequest(Page request);

			Microsoft::WRL::ComPtr<ID3D12Resource> mRenderTargetBuffer = nullptr;
			Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer = nullptr;
			const VTDesc* mInfo = nullptr;
			D3D12_VIEWPORT mViewport{};
			size_t mSize = 0;

			PageIndexer mIndexer{};

			std::unordered_map<uint32_t, uint32_t> mRequests{};

			StagingBufferPool mResources{};
		};		

		struct PageCount
		{
			Page page;
			uint32_t count;

			static bool Comparator(const PageCount& lhs, const PageCount& rhs)
			{
				if (rhs.page.Mip == lhs.page.Mip)
				{
					if (rhs.count < lhs.count)
					{
						return true;
					}
				}
				if (rhs.page.Mip < lhs.page.Mip)
				{
					return true;
				}
				return false;
			}
		};

		template<typename Generator>
		// All the data management relating to Virtual Textures
		class VirtualTexture
		{
		public:
			using GenerationTextures = GpuPageGenerator<Generator>::GenerationTextures;
			static constexpr uint32_t TEXTURES_COUNT() { return GpuPageGenerator<Generator>::TEXTURES_COUNT(); }
		public:
			VirtualTexture() {}
			virtual ~VirtualTexture() {}

			inline int32_t GetAtlasTexLocation(uint32_t id) const
			{
				return mTextureAtlas->GetTexId(id);
			}
			inline int32_t GetIndirectionTexId() const
			{
				return mPageTable->GetTexId();
			}
			inline GpuPageGenerator<Generator>& GetPageLoader() const
			{
				return *mLoader;
			}
			inline Generator& GetGenerator() const
			{
				return *((Generator*)mLoader.get());
			}
			inline int32_t GetPageIndex(const Page& p) const
			{
				if (mIndexer == nullptr) return -1;
				return mIndexer->PageIndex(p);
			}

			inline uint32_t GetBias() { return mMipBias; }
			inline void SetBias(uint32_t newValue) { mMipBias = newValue; }

			inline virtual void Init
			(
				Microsoft::WRL::ComPtr<ID3D12Device> device,
				std::array<Texture*, Generator::TEXTURES_COUNT()>& atlasTexture,
				Texture* const& indirectionTexture,
				const VTDesc* info,
				uint32_t uploadsPerFrame,
				std::unique_ptr<ComputeContext>& computeContext
			)
			{
				mInfo = info;
				mUploadsPerFrame = uploadsPerFrame;

				mIndexer = std::make_unique<PageIndexer>();
				mIndexer->Init(*mInfo);

				mLoader = std::make_unique<Generator>();
				mLoader->Init(mIndexer.get(), mInfo, std::move(computeContext), 1);

				mTextureAtlas = std::make_unique<TextureAtlas<Generator::TEXTURES_COUNT()>>();
				mTextureAtlas->Init
				(
					device, 
					atlasTexture,
					Generator::TEXTURES_FORMAT(), 
					Generator::TEXTURES_BYTES_PER_TEXEL(),
					mInfo,
					mUploadsPerFrame,
					mInfo->AtlasTilesPerRow
				);

				mCache = std::make_unique<PageCache>();
				mCache->OnPageRequestedToAdd([&](const Page& p) { mLoader->Submit(p); });
				mCache->OnPageDataComputedUpload([&](Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, const Point& position, std::vector<data_ptr>& data)
					{
						mTextureAtlas->UploadPage(commandList, position, data);
					});
				mCache->OnPageAddedToCache([&](const Page& request, const Point& mapping) { mPageTable->AddPage(request, mapping); });
				mCache->OnPageRemovedFromCache([&](const Page& request, const Point& mapping) { mPageTable->RemovePage(request); });
				mCache->Init(mInfo->AtlasTilesPerRow);
				mLoader->OnLoadComplete([&](Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, const MultiPage& mp) { mCache->LoadComplete(cmdList, mp); });

				mPageTable = std::make_unique<PageTableIndirection>();
				mPageTable->Init(device, info, indirectionTexture);
			}

			inline virtual void Update
			(
				Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
				const std::unordered_map<uint32_t, uint32_t>& requests
			)
			{
				const size_t atlas2 = (size_t)mInfo->AtlasTilesPerRow * mInfo->AtlasTilesPerRow;
				std::vector<PageCount> toLoad{};

				//size_t updated = 0;
				for (const auto& [page_index, num_requests] : requests)
				{
					const Page p = mIndexer->GetPage(page_index);

					// Not updated is the same as say that not contains. If the cache updates the page is because it's contained.
					if (!mCache->UpdatePagePosition(p))
					{
						toLoad.push_back(PageCount{ .page = std::move(p), .count = num_requests });
					}
				}

				const size_t updated = requests.size() - toLoad.size();
				if (updated < atlas2)
				{
					std::sort(toLoad.begin(), toLoad.end(), PageCount::Comparator);

					const uint32_t loadCount = (uint32_t)min(min(toLoad.size(), (size_t)mUploadsPerFrame), atlas2);

					for (uint32_t i = 0; i < loadCount; ++i)
					{
						mCache->Request(toLoad[i].page);
					}
				}
				else
				{
					--mMipBias;
				}

				mLoader->Update(commandList, mUploadsPerFrame);
				mPageTable->Update(commandList);
			}

			inline virtual void Clear(Microsoft::WRL::ComPtr<ID3D12Device> device, DescriptorHeaps& descriptorHeaps)
			{
				mLoader->Clear();
				mCache->Clear();
				mTextureAtlas->Clear(device, descriptorHeaps, Generator::TEXTURES_FORMAT());
				mPageTable->Clear(device, descriptorHeaps);
				mLoader->Restart();
			}

		protected:
			const VTDesc*                                                                mInfo         = nullptr;
			std::unique_ptr<PageIndexer>                                                 mIndexer      = nullptr;
			std::unique_ptr<PageTableIndirection>                                        mPageTable    = nullptr;
			std::unique_ptr<TextureAtlas<GpuPageGenerator<Generator>::TEXTURES_COUNT()>> mTextureAtlas = nullptr;
			std::unique_ptr<GpuPageGenerator<Generator>>                                 mLoader       = nullptr;
			std::unique_ptr<PageCache>                                                   mCache        = nullptr;
			uint32_t mMipBias = 0;
			uint32_t mUploadsPerFrame = 0;
		};

	}
}


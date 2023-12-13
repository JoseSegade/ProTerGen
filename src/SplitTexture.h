#pragma once

#include "CommonHeaders.h"
#include <unordered_set>
#include <array>
#include <thread>
#include "Materials.h"
#include "Texture.h"
#include "ConcurrentQueue.h"
#include "LRUCache.h"

namespace ProTerGen
{
	namespace ST
	{
		struct STDesc
		{
			uint16_t STWidth           = 0;
			uint8_t  TilesPerSideExp   = 0;
			uint8_t  AtlasTilesPerSide = 0;

			constexpr uint32_t TilesPerSide() const
			{
				return ((uint32_t)1 << TilesPerSideExp);
			}
			constexpr uint32_t TileWidth() const
			{
				return (uint32_t)STWidth / TilesPerSide();
			}
			constexpr uint32_t NumTiles() const
			{
				return TilesPerSide() * TilesPerSide();
			}
			constexpr uint32_t AtlasNumTiles() const
			{
				return AtlasTilesPerSide * AtlasTilesPerSide;
			}
			constexpr uint32_t AtlasWidth() const
			{
				return AtlasTilesPerSide * TileWidth();
			}
		};

		struct SplitTextureInfo
		{
			uint8_t      numChannels     = 1;
			uint8_t      bytesPerChannel = 1;
			DXGI_FORMAT  format          = DXGI_FORMAT_R8_TYPELESS;
			std::wstring path            = L"";

			constexpr uint32_t TexelBytes() const
			{
				return (uint32_t)numChannels * bytesPerChannel;
			}
		};

		struct Page
		{
			uint16_t x = 0xffff;
			uint16_t y = 0xffff;

			constexpr uint32_t hash() const
			{
				return (x << 16) ^ (y);
			}
			constexpr uint32_t isValid() const
			{
				return (x != 0xffff) && (y != 0xffff);
			}
			constexpr bool operator==(const Page& rhs) const
			{
				return (x == rhs.x) && (y == rhs.y);
			}
		};

		struct PageData
		{
			Page page{};
			void* data = nullptr;
		};

		class AsyncWorker; 
		class TextureLoader; 
		class PhysicalTexture;
		struct TextureProcessingUnit
		{
			std::unique_ptr<AsyncWorker>     Worker;
			std::unique_ptr<TextureLoader>   Loader;
			std::unique_ptr<PhysicalTexture> PhysicalTexture;
		};
	}
}

template<>
struct std::hash<ProTerGen::ST::Page>
{
	constexpr size_t operator()(const ProTerGen::ST::Page& p) const noexcept
	{
		return p.hash();
	}
};

namespace ProTerGen
{
	namespace ST
	{
		class TextureLoader
		{
		public:
			TextureLoader() {};
			virtual ~TextureLoader() { Close(); }
			void Init(STDesc* desc, SplitTextureInfo* info);
			void CreateNewFile();
			bool OpenReadingFromPath();
			bool Read(PageData& page);
			bool Write(const PageData& page);
			void DeleteFileTexture();
			void Close();
		private:
			FILE* mFile{};
			FILE* mWriter{};
			STDesc* mDesc = nullptr;
			SplitTextureInfo* mInfo = nullptr;
		};

		class AsyncWorker
		{
		public:
			using request_func_t  = std::function<void(Page& page)>;
			using complete_func_t = std::function<void(PageData& page)>;

			AsyncWorker() {};
			virtual ~AsyncWorker() {};

			void Init();
			void Request(Page& page);

			void OnRequest(request_func_t& function) { mOnRequest = function; }
			void OnComplete(complete_func_t& function) { mOnComplete = function; }

			void ExitSignal();
		private:
			request_func_t  mOnRequest  = [](Page&) {};
			complete_func_t mOnComplete = [](PageData&) {};

			std::condition_variable mCond;
			std::jthread mThread;
			std::atomic_bool mIsRunning = false;
			std::atomic_int mSemaphore = 0;
			std::mutex mMutex;

			NBConcurrentQueue<Page> mRequested;
			NBConcurrentQueue<PageData> mCompleted;
		};

		class Cache
		{
		public:
			using submit_func_t = std::function<void(const Page& p)>;
			using upload_func_t = std::function<void(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>, const Point&, void*&)>;
			using add_func_t    = std::function<void(const Page&, const Point&)>;
			using remove_func_t = std::function<void(const Page&, const Point&)>;

			void OnPageRequestedToAdd(submit_func_t func) { mSubmit = func; }
			void OnPageDataComputedUpload(upload_func_t func) { mUploadData = func; }
			void OnPageAddedToCache(add_func_t func) { mOnPageAdded = func; }
			void OnPageRemovedFromCache(remove_func_t func) { mOnRemove = func; }

			void Init(STDesc* desc);
			bool UpdatePage(Page& p);
			bool RequestPage(Page& p);

			void LoadComplete(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, PageData& page);
			void Clear();
		private:
			submit_func_t mSubmit      = [](const Page&) {};
			upload_func_t mUploadData  = [](Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>, const Point&, void*&) {};
			add_func_t    mOnPageAdded = [](const Page&, const Point&) {};
			remove_func_t mOnRemove    = [](const Page&, const Point&) {};

			STDesc*                  mDesc = nullptr;
			LRUCache<Page, Point>    mLru{};
			std::unordered_set<Page> mLoading{};

			uint32_t                 mCurrent = 0;
		};

		class PhysicalTexture
		{
		public:
			void Init(Microsoft::WRL::ComPtr<ID3D12Device> device, DescriptorHeaps& descriptorHeaps, Texture* texture, STDesc* desc, SplitTextureInfo* info);
			void Update(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList);
			void Write(const void*& pageData, const Point& point);
			void Clear(Microsoft::WRL::ComPtr<ID3D12Device> device, DescriptorHeaps& descriptorHeaps);
		private:
			bool              mIsDirty = false;
			STDesc*           mDesc    = nullptr; 
			SplitTextureInfo* mInfo    = nullptr;
			Texture*          mTexture = nullptr;
		};

		class IndirectionTexture
		{
		public:
			void Init
			(
				Microsoft::WRL::ComPtr<ID3D12Device> device,
				DescriptorHeaps& descriptorHeaps,
				Texture* texture,
				STDesc* desc
			);
			void Update(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList);
			void AddPage(const Page& page);
			void Clear(Microsoft::WRL::ComPtr<ID3D12Device> device, DescriptorHeaps& descriptorHeaps);
		private:
			bool     mIsDirty = false;
			STDesc*  mDesc    = nullptr;
			Texture* mTexture = nullptr;
		};

		class SplitTexture
		{
		public:
			SplitTexture() {};
			virtual ~SplitTexture() {};

			void Init
			(
				Microsoft::WRL::ComPtr<ID3D12Device> device,
				STDesc& desc,
				std::vector<SplitTextureInfo>& textureInfo,
				Materials& materials,
				DescriptorHeaps& descriptorHeaps
			);
			void AddNewTexture
			(
				Microsoft::WRL::ComPtr<ID3D12Device> device,
				SplitTexture& textureInfo,
				Materials& materials,
				DescriptorHeaps& descriptorHeaps
			);
			void Request(std::vector<Point>& points);
			void Update(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList);
			void Clear();
		private:
			STDesc mDesc;
			std::vector<TextureProcessingUnit> mProcesingUnits{};
			std::unique_ptr<IndirectionTexture> mIndirectionTexture = nullptr;
			std::vector<SplitTextureInfo> mTexturesInfo{};
		};
	}
}

#pragma once

#include "CommonHeaders.h"

#include <unordered_set>

namespace ProTerGen
{
	namespace VT
	{
		struct Page
		{
			uint32_t X = 0xffffffff;
			uint32_t Y = 0xffffffff;
			uint32_t Mip = 0xffffffff;

			inline bool operator==(const Page& rhs) const { return X == rhs.X && Y == rhs.Y && Mip == rhs.Mip; };
		};
	}
}

template<>
struct std::hash<ProTerGen::VT::Page>
{
	size_t operator()(ProTerGen::VT::Page const& page) const noexcept
	{
		const size_t s1 = std::hash<uint32_t>{}(page.X);
		const size_t s2 = std::hash<uint32_t>{}(page.Y);
		const size_t s3 = std::hash<uint32_t>{}(page.Mip);

		return s3 ^ (s2 << 4) ^ (s1 << 32);
	}
};

namespace ProTerGen
{
	namespace VT
	{
		typedef uint32_t PageIndex;
		typedef void* data_ptr;
		typedef unsigned char* byte_ptr;
		typedef unsigned char byte;

		const DXGI_FORMAT VT_HEIGHTMAP_FORMAT = DXGI_FORMAT_R16_FLOAT;
		const uint32_t VT_HEIGHTMAP_BYTES_PER_PIXEL = 2;
		const DXGI_FORMAT VT_INDIRECTION_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
		const uint32_t VT_INDIRECTION_BYTES_PER_PIXEL = 4;
		const DXGI_FORMAT VT_COLOR_ALBEDO_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
		const uint32_t VT_COLOR_ALBEDO_BYTES_PER_PIXEL = 4;
		const DXGI_FORMAT VT_NORMAL_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
		const uint32_t VT_NORMAL_BYTES_PER_PIXEL = 4;

		struct VTDesc
		{
			uint32_t VTSize = 0;
			uint32_t VTTilesPerRowExp = 0;
			uint32_t AtlasTilesPerRow = 0;
			uint32_t BorderSize = 0;

			constexpr uint32_t VTTilesPerRow() const { return (1 << VTTilesPerRowExp); }
			constexpr uint32_t TileSize() const { return VTSize / (1 << VTTilesPerRowExp); }
			constexpr uint32_t BorderedTileSize() const { return TileSize() + 2 * BorderSize; }
			constexpr uint32_t AtlasSize() const { return BorderedTileSize() * AtlasTilesPerRow; }
		};

		class PageIndexer
		{
		public:
			void Init(const VTDesc& desc);

			PageIndex PageIndex(const Page& page) const;
			const Page& GetPage(uint32_t idx) const;

			constexpr bool IsValid(const Page& page) const
			{
				return (page.Mip < mMipCount) && (page.X < mSizes[page.Mip]) && (page.Y < mSizes[page.Mip]);
			}

			constexpr size_t GetCount() const { return mCount; }
		private:
			uint32_t mMipCount = 0;
			size_t mCount = 0;
			std::vector<size_t> mOffsets;
			std::vector<size_t> mSizes;
			std::vector<Page> mReverse;
		};

		class LightPageIndexer
		{
		public:
			void Init(uint32_t maxMipCount);
			PageIndex PageIndex(const Page& page) const;
			Page GetPage(uint32_t idx) const;
		private:
			uint32_t mMaxMip = 0;
		};


		struct ReadState
		{
			Page page = {};
			mutable data_ptr data = nullptr;

			ReadState() : page(), data(nullptr) {}

			ReadState(const ReadState& lhs)
			{
				page = lhs.page;
				data = lhs.data;
				lhs.data = nullptr;
			}

			ReadState& operator=(const ReadState& lhs)
			{
				page = lhs.page;
				data = lhs.data;
				lhs.data = nullptr;
				return *this;
			}

			~ReadState()
			{
				if (data)
				{
					free(data);
					data = nullptr;
				}
			}
		};

		struct MultiPage
		{
			Page page = {};
			mutable std::vector<data_ptr> dataPtrs;

			MultiPage() : page(), dataPtrs() {};
			MultiPage(const Page& p, std::vector<data_ptr>& dptrs) : page(p), dataPtrs(std::move(dptrs)) {}

			MultiPage(const MultiPage& lhs)
			{
				page = lhs.page;
				dataPtrs = std::move(lhs.dataPtrs);
				lhs.dataPtrs.clear();
			}

			MultiPage& operator=(const MultiPage& lhs)
			{
				page = lhs.page;
				dataPtrs = std::move(lhs.dataPtrs);
				lhs.dataPtrs.clear();
				return *this;
			}

			~MultiPage()
			{
				for (auto& i : dataPtrs)
				{
					if (i != nullptr)
					{
						free(i);
						i = nullptr;
					}
				}
			}
		};
	}
}
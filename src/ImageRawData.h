#pragma once

#include "CommonHeaders.h"
#include "MathHelpers.h"

namespace ProTerGen
{
	struct FileRawData
	{
		mutable FILE* File = nullptr;
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t FormatSize = 0;
		int32_t Offset = -1;

		~FileRawData();

		void RewindPtr() const;

		// SRC region width and height should be the same as the dest pointer.
		void CopyTo(void* dst, const Point& destOffset, const Rectangle& srcRegion) const;
		void WriteIn(const void* src, const Point& destOffset, const Rectangle& srcRegion) const;
	};

	class Fillable
	{
	public:
		virtual void FillRegion(const Rectangle& region, const void* color) = 0;
	};

	struct ImageRawData : public Fillable
	{
		void* Data = nullptr;
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t FormatSize = 0;

		~ImageRawData();

		void Init(uint32_t width, uint32_t height, uint32_t formatSize);
		
		void CopyTo(ImageRawData& dst, const Point& dstOffset, const Rectangle& srcRegion) const;
		void CopyTo(void* dst, const Point& dstOffset, const Rectangle& srcRegion) const;
		void FillRegion(const Rectangle& region, const void* color);

		void Mipmap(void* dest, const Point& dstOffset, uint32_t mipSize, uint32_t channels) const;

		void Clear();		
	};

	struct ImageRawDataFootprint : public Fillable
	{
		void* Data = nullptr;
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint;
		uint32_t FormatSize = 0;

		void Init(void* data, const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& footprint, uint32_t formatSize);

		void CopyTo(ImageRawData& dst, const Point& dstOffset, const Rectangle& srcRegion) const;
		void CopyTo(void* dst, const Point& dstOffset, const Rectangle& srcRegion) const;
		void FillRegion(const Rectangle& region, const void* color);
	};
}

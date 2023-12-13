#include "VirtualTextureCommon.h"
#include "MathHelpers.h"

void ProTerGen::VT::PageIndexer::Init(const VTDesc& desc)
{
	/*
	*  Page: Coords(x, y) at mip N.
	*
	*  Mip 0 (size = 4 * 4, offset = 0) :
	*  +----------+-----------+-----------+-----------+
	*  |          |           |           |           |
	*  |    0     |     1     |     2     |     3     |
	*  |          |           |           |           |
	*  +----------+-----------+-----------+-----------+
	*  |          |           |           |           |
	*  |    4     |     5     |     6     |     7     |
	*  |          |           |           |           |
	*  +----------+-----------+-----------+-----------+
	*  |          |           |           |           |
	*  |    8     |     9     |    10     |    11     |
	*  |          |           |           |           |
	*  +----------+-----------+-----------+-----------+
	*  |          |           |           |           |
	*  |    12    |     13    |     14    |     15    |
	*  |          |           |           |           |
	*  +----------+-----------+-----------+-----------+
	*
	*  Mip 1 (size = 2 * 2, offset = 16) :
	*  +----------+-----------+
	*  |          |           |
	*  |    16    |     17    |
	*  |          |           |
	*  +----------+-----------+
	*  |          |           |
	*  |    18    |     19    |
	*  |          |           |
	*  +----------+-----------+
	*
	*  Mip 2 (size = 1 * 1, offset = 20):
	*  +----------+
	*  |          |
	*  |    20    |
	*  |          |
	*  +----------+
	*/


	mMipCount = desc.VTTilesPerRowExp + 1;

	mSizes.resize(mMipCount);
	mOffsets.resize(mMipCount);
	mCount = 0;
	for (uint32_t i = 0; i < mMipCount; ++i)
	{
		mSizes[i] = (desc.VTSize / desc.TileSize()) >> i;
		mOffsets[i] = mCount;
		mCount += (size_t)mSizes[i] * mSizes[i];
	}

	mReverse.resize(mCount);
	for (uint32_t i = 0; i < mMipCount; ++i)
	{
		size_t size = mSizes[i];
		for (size_t y = 0; y < size; ++y)
		{
			for (size_t x = 0; x < size; ++x)
			{
				Page p = { .X = (uint32_t)x, .Y = (uint32_t)y, .Mip = i };
				mReverse[PageIndex(p)] = p;
			}
		}
	}
}

ProTerGen::VT::PageIndex ProTerGen::VT::PageIndexer::PageIndex(const Page& page) const
{
	assert(page.Mip < mMipCount);

	const size_t offset = mOffsets[page.Mip];
	const size_t stride = mSizes[page.Mip];

	return (uint32_t)(offset + page.Y * stride + page.X);
}

const ProTerGen::VT::Page& ProTerGen::VT::PageIndexer::GetPage(uint32_t idx) const
{
	return mReverse.at(idx);
}

void ProTerGen::VT::LightPageIndexer::Init(uint32_t maxMipCount)
{
	mMaxMip = maxMipCount;
}

ProTerGen::VT::PageIndex ProTerGen::VT::LightPageIndexer::PageIndex(const Page& page) const
{
	const uint32_t maxOffset = (4 * ((1 << 2 * (mMaxMip)) - 1)) / 3;
	const uint32_t offset = (4 * ((1 << 2 * (mMaxMip - (page.Mip))) - 1)) / 3;
	const uint32_t diff = maxOffset - offset;
	const uint32_t size = 1 << mMaxMip - (page.Mip);
	return diff + page.Y * size + page.X;
}

ProTerGen::VT::Page ProTerGen::VT::LightPageIndexer::GetPage(uint32_t idx) const
{
	const uint32_t maxOffset = (4 * ((1 << 2 * (mMaxMip)) - 1)) / 3;
	if (idx == maxOffset) return Page{ .X = 0, .Y = 0, .Mip = mMaxMip };
	const uint32_t reIdx = maxOffset - idx;

	const uint32_t lod = 1 + (FastLog2((uint32_t)((1.0f + 3.0f * reIdx) / 4.0f)) / 2);
	const uint32_t offset = (4 * ((1 << 2 * (lod)) - 1)) / 3;
	const uint32_t diff = maxOffset - offset;
	const uint32_t newIndex = idx - diff;
	const uint32_t size = 1 << (lod);
	return Page{ .X = (uint16_t)(newIndex % size), .Y = (uint16_t)(newIndex / size), .Mip = (uint8_t)(mMaxMip - lod) };
}


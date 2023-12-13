#pragma once

#include "CommonHeaders.h"

#include <DirectXMath.h>
#include <array>
#include <functional>
#include <unordered_set>

#include "ConcurrentQueue.h"
#include "MathHelpers.h"
#include "ImageRawData.h"
#include "Texture.h"
#include "DescriptorHeaps.h"
#include "PointerHelper.h"
#include "LRUCache.h"
#include "PageThread.h"

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
}
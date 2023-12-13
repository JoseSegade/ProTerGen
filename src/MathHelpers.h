#pragma once

#include "CommonHeaders.h"
#include <functional>
#include <fstream>

#ifndef max
#define max(x, y) ((x) > (y) ? (x) : (y))
#endif
#ifndef min
#define min(x, y) ((x) < (y) ? (x) : (y))
#endif
#ifndef ProTerGen_clamp
#define ProTerGen_clamp(n, m, v) (max((n), min((m), (v))))
#endif
#ifndef Log2
#define Log2(x) (uint32_t)log2(((int32_t)(x)))
#endif


namespace ProTerGen
{
	static const float COS_QUARTER_PI = 0.707106781f;
	static const float SQRT2          = 1.414213562f;
	static const float NUMBER_E       = 2.718281828f;
	static const float NUMBER_PI      = 3.141692653f;

	struct Point
	{
		uint32_t X = 0;
		uint32_t Y = 0;
	};

	struct Rectangle
	{
		uint32_t X = 0;
		uint32_t Y = 0;
		uint32_t Width = 0;
		uint32_t Height = 0;

		constexpr bool Contains(const Point& p) const
		{
			return X <= p.X && p.X < (X + Width) && Y <= p.Y && p.Y < (Y + Height);
		}

		constexpr bool Containts(const Rectangle& r) const
		{
			return (X <= r.X) && (X + Width > r.X + r.Width) && (Y <= r.Y) && (Y + Height > r.Y + r.Height);
		}
	};

	static inline uint32_t Modulus(int32_t a, int32_t b) noexcept
	{
		const float _a = (float)a;
		const float _b = (float)b;
		return (uint32_t)(_a - _b * floorf(_a / _b));
	}

	constexpr size_t Align(size_t size, size_t alignment = 255) noexcept
	{
		return (size + alignment) & ~alignment;
	}

	template<typename T>
	constexpr T Lerp(const T& p1, const T& p2, float weight) noexcept
	{
		weight = ProTerGen_clamp(0.0, 1.0, weight);
		return weight * (p2 - p1) + p1;
	}

	template<typename T>
	constexpr T InvLerp(const T& p1, const T& p2, float weight) noexcept
	{
		weight = ProTerGen_clamp(0.0, 1.0, weight);
		return (weight - p1) / (p2 - p1);
	}

	constexpr uint32_t FastLog2(uint32_t val) noexcept
	{
		const uint32_t tab32[32] =
		{
			 0,  9,  1, 10, 13, 21,  2, 29,
			11, 14, 16, 18, 22, 25,  3, 30,
			 8, 12, 20, 28, 15, 17, 24,  7,
			19, 27, 23,  6, 26,  5,  4, 31
		};
		val |= val >> 1;
		val |= val >> 2;
		val |= val >> 4;
		val |= val >> 8;
		return tab32[(uint32_t)(val * 0x07c4acdd) >> 27];
	}

	constexpr uint32_t BitScanOne(uint32_t val) noexcept
	{
		uint32_t i = 0;
		while (i < 32)
		{
			if (((1 << i) & val) == (1 << i)) return i;
			++i;
		}
		return i;
	}

	static std::string Wstring2String(const std::wstring& wstr)
	{
		if (wstr == L"") return "";
		std::locale loc{};
		const wchar_t* wideptr = wstr.c_str();
		std::size_t len = std::wcslen(wideptr);
		std::vector<char> narrowstr(len);
		std::use_facet<std::ctype<wchar_t>>(loc).narrow(wideptr, wideptr + len, '?', &narrowstr[0]);
		narrowstr.push_back('\0');
		return std::string(narrowstr.data());
	}
	
	constexpr float Map(float oldMin, float oldMax, float newMin, float newMax, float value)
	{
		const float oldDiff = oldMax - oldMin;
		const float newDiff = newMax - newMin;
		return (((value - oldMin) / oldDiff) * newDiff) + newMin;
	}

	constexpr std::string _BuildUniqueId(size_t classIdHash, uint32_t entity, uint32_t number) noexcept
	{
		return std::to_string(classIdHash) + "_" + std::to_string(entity) + "_" + std::to_string(number);
	}

	#define __CLASS__ typeid(decltype(*this)).hash_code()
	#define BuildUniqueId(x, y) _BuildUniqueId(__CLASS__, (x), (y))
}
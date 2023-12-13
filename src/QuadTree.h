#pragma once

#include <array>
#include <DirectXCollision.h>
#include "CommonHeaders.h"
#include "MathHelpers.h"

namespace ProTerGen
{
	class RQuadTreeTerrain
	{

	public:
		enum class Type : uint32_t
		{
			ROOT,
			NORTHEAST,
			NORTHWEST,
			SOUTHEAST,
			SOUTHWEST
		};

		enum Border : uint8_t
		{
			NONE = 0,
			NORTH = 1 << 0,
			EAST = 1 << 1,
			SOUTH = 1 << 2,
			WEST = 1 << 3,
			NORTHEAST = NORTH | EAST,
			NORTHWEST = NORTH | WEST,
			SOUTHEAST = SOUTH | EAST,
			SOUTHWEST = SOUTH | WEST
		};

		static const size_t BORDER_COUNT = 9;
		static constexpr size_t ToNumeral(Border b)
		{
			switch (b)
			{
			case Border::NONE:
				return 0;
			case Border::NORTH:
				return 1;
			case Border::EAST:
				return 2;
			case Border::SOUTH:
				return 3;
			case Border::WEST:
				return 4;
			case Border::NORTHEAST:
				return 5;
			case Border::NORTHWEST:
				return 6;
			case Border::SOUTHEAST:
				return 7;
			case Border::SOUTHWEST:
				return 8;
			}
			return 0;
		}
		static constexpr std::array<Border, BORDER_COUNT> GetBorders()
		{
			return { Border::NONE, Border::NORTH, Border::EAST, Border::SOUTH, Border::WEST, Border::NORTHEAST, Border::NORTHWEST, Border::SOUTHEAST, Border::SOUTHWEST };
		}
		static constexpr bool IsCorner(Border b) { return b != Border::NONE && !IsBorder(b); }
		static constexpr bool IsBorder(Border b) { return b == Border::NORTH || b == Border::EAST || b == Border::SOUTH || b == Border::WEST; }
		static constexpr bool ContainsBorder(Border b, Border equals) { return ((uint8_t)b & (uint8_t)equals) == (uint8_t)equals; }
		static constexpr bool ContainsNorth(Border b) { return ContainsBorder(b, Border::NORTH); }
		static constexpr bool ContainsEast(Border b) { return ContainsBorder(b, Border::EAST); }
		static constexpr bool ContainsSouth(Border b) { return ContainsBorder(b, Border::SOUTH); }
		static constexpr bool ContainsWest(Border b) { return ContainsBorder(b, Border::WEST); }

		enum class Corner : uint8_t
		{
			NONE = 0,
			NE = 2,
			NW = 1,
			SE = 3,
			SW = 4
		};

		RQuadTreeTerrain
		(
			Type type,
			RQuadTreeTerrain* parent,
			float centerX,
			float centerY,
			float edgeSize
		);

		virtual ~RQuadTreeTerrain();

		void Reset();

		void Subdivide
		(
			DirectX::XMFLOAT3 pos,
			const DirectX::BoundingFrustum& frustum,
			std::vector<RQuadTreeTerrain*>& leafNodes,
			float height,
			float chunkSize = 16
		);
		void UpdateNeigbours();

		constexpr float EdgeLength() const { return mEdgeSize; };
		constexpr float HalfEdgeLength() const { return mEdgeSize * .5f; };
		constexpr float QuarterEdgeLength() const { return mEdgeSize * .25f; };
		constexpr float GetMinX() const { return mCenterX - HalfEdgeLength(); };
		constexpr float GetMinY() const { return mCenterY - HalfEdgeLength(); };
		constexpr float GetCenterX() const { return mCenterX; }
		constexpr float GetCenterY() const { return mCenterY; }
		constexpr float GetDistance() const { return mDistance; }
		constexpr uint32_t GetDepth() const { return mDepth; }

		inline Type GetType() const { return mType; }

		Border GetBorder() const;
		inline bool HasBorder(Border b) const {return ((uint8_t)GetBorder() & (uint8_t)b) == (uint8_t)b;};
		Corner GetCorner() const;

	protected:
		std::unique_ptr<RQuadTreeTerrain> mNE;
		std::unique_ptr<RQuadTreeTerrain> mNW;
		std::unique_ptr<RQuadTreeTerrain> mSE;
		std::unique_ptr<RQuadTreeTerrain> mSW;

		RQuadTreeTerrain* mNeighbourNorth;
		RQuadTreeTerrain* mNeighbourEast;
		RQuadTreeTerrain* mNeighbourSouth;
		RQuadTreeTerrain* mNeighbourWest;

		RQuadTreeTerrain* mParent;
		
		float mDistance = 0.0f;
		uint32_t mDepth = 0;

		float mCenterX = 0;
		float mCenterY = 0;
		float mEdgeSize = 0;

		bool mIsDivided = false;
		Type mType = Type::ROOT;
	};
}
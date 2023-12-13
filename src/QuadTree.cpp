#include "QuadTree.h"
#include <limits>
#include <ppl.h>
#include "PointerHelper.h"

ProTerGen::RQuadTreeTerrain::RQuadTreeTerrain
(
	RQuadTreeTerrain::Type type,
	RQuadTreeTerrain* parent,
	float centerX,
	float centerY,
	float edgeSize
)
	: mType(type)
	, mParent(parent)
	, mCenterX(centerX)
	, mCenterY(centerY)
	, mEdgeSize(edgeSize)
	, mIsDivided(false)
	, mNeighbourNorth(nullptr)
	, mNeighbourEast(nullptr)
	, mNeighbourSouth(nullptr)
	, mNeighbourWest(nullptr)
	, mDistance(0.0f)
{
	if (mParent == nullptr)
	{
		assert(mType == RQuadTreeTerrain::Type::ROOT && "Not-root Quad Tree nodes must have a parent attached.");
		mDepth = 0;
	}
	else
	{
		mDepth = mParent->mDepth + 1;
	}
}

ProTerGen::RQuadTreeTerrain::~RQuadTreeTerrain()
{
	Reset();
}

void ProTerGen::RQuadTreeTerrain::Reset()
{
	mParent = nullptr;
	mNE.reset();
	mNW.reset();
	mSE.reset();
	mSW.reset();
	mNeighbourNorth = nullptr;
	mNeighbourEast = nullptr;
	mNeighbourSouth = nullptr;
	mNeighbourWest = nullptr;
}

void ProTerGen::RQuadTreeTerrain::Subdivide
(
	DirectX::XMFLOAT3 pos,
	const DirectX::BoundingFrustum& frustum,
	std::vector<RQuadTreeTerrain*>& leafNodes,
	float height,
	float minEdgeLength
)
{
	const float disX = mCenterX - pos.x;
	const float disY = mCenterY - pos.z;
	//float distance = (abs(round(disX)) + abs(round(disY))); // manhattan distance
	const float distance = sqrt((disX * disX) + (disY * disY)); // euclidean distance

	const float halfMinEdge = minEdgeLength * 0.5f;
	//const float halfMinEdge2 = halfMinEdge * halfMinEdge;
	//const float halfMinRadius = sqrt(halfMinEdge2 + halfMinEdge2);
	//const float edgeLength2 = EdgeLength() * EdgeLength();
	//const float quadRadius = sqrt(edgeLength2 + edgeLength2);
	const float halfMinRadius = SQRT2 * halfMinEdge;
	const float quadRadius = SQRT2 * (EdgeLength() * 2 - halfMinEdge);

	mDistance = std::abs(distance - halfMinRadius);

	bool intersects = frustum.Intersects(DirectX::BoundingBox
	(
		//DirectX::XMFLOAT3(pos.x + mCenterX, 500.f, pos.z + mCenterY),
		DirectX::XMFLOAT3(mCenterX, height, mCenterY),
		DirectX::XMFLOAT3(HalfEdgeLength() + 0.001f, height + 0.001f, HalfEdgeLength() + 0.001f)
	));
	//bool isEnoughDistance = (distance - halfMinRadius < quadRadius * 2);
	bool isEnoughDistance = (mDistance < quadRadius);

	if (intersects && EdgeLength() > minEdgeLength && isEnoughDistance)
	{
		if (!mIsDivided)
		{
			const float quarterSize = QuarterEdgeLength();
			const float halfSize = HalfEdgeLength();
			const float cx = mCenterX;
			const float cy = mCenterY;

			mNE = std::make_unique<RQuadTreeTerrain>
				(
					RQuadTreeTerrain::Type::NORTHEAST,
					this,
					cx + quarterSize,
					cy + quarterSize,
					halfSize
					);
			mNW = std::make_unique<RQuadTreeTerrain>
				(
					RQuadTreeTerrain::Type::NORTHWEST,
					this,
					cx - quarterSize,
					cy + quarterSize,
					halfSize
					);
			mSE = std::make_unique<RQuadTreeTerrain>
				(
					RQuadTreeTerrain::Type::SOUTHEAST,
					this,
					cx + quarterSize,
					cy - quarterSize,
					halfSize
					);
			mSW = std::make_unique<RQuadTreeTerrain>
				(
					RQuadTreeTerrain::Type::SOUTHWEST,
					this,
					cx - quarterSize,
					cy - quarterSize,
					halfSize
					);

			mIsDivided = true;
		}

		mNE->Subdivide(pos, frustum, leafNodes, height,minEdgeLength);
		mNW->Subdivide(pos, frustum, leafNodes, height,minEdgeLength);
		mSE->Subdivide(pos, frustum, leafNodes, height,minEdgeLength);
		mSW->Subdivide(pos, frustum, leafNodes, height,minEdgeLength);
	}
	else
	{
		if (mIsDivided)
		{
			mNE.reset();
			mNW.reset();
			mSE.reset();
			mSW.reset();

			mIsDivided = false;
		}

		if (intersects)
		{
			leafNodes.push_back(this);
		}
	}
}

void ProTerGen::RQuadTreeTerrain::UpdateNeigbours()
{

	switch (mType)
	{
	case RQuadTreeTerrain::Type::NORTHWEST:
		mNeighbourNorth = TRY_GET_PTR(mParent->mNeighbourNorth, mSW.get());
		mNeighbourEast = mParent->mNE.get();
		mNeighbourSouth = mParent->mSW.get();
		mNeighbourWest = TRY_GET_PTR(mParent->mNeighbourWest, mNE.get());
		break;
	case RQuadTreeTerrain::Type::NORTHEAST:
		mNeighbourNorth = TRY_GET_PTR(mParent->mNeighbourNorth, mSE.get());
		mNeighbourEast = TRY_GET_PTR(mParent->mNeighbourEast, mNW.get());
		mNeighbourSouth = mParent->mSE.get();
		mNeighbourWest = mParent->mNW.get();
		break;
	case RQuadTreeTerrain::Type::SOUTHEAST:
		mNeighbourNorth = mParent->mNE.get();
		mNeighbourEast = TRY_GET_PTR(mParent->mNeighbourEast,mSW.get());
		mNeighbourSouth = TRY_GET_PTR(mParent->mNeighbourSouth, mNE.get());
		mNeighbourWest = mParent->mSW.get();
		break;
	case RQuadTreeTerrain::Type::SOUTHWEST:
		mNeighbourNorth = mParent->mNW.get();
		mNeighbourEast = mParent->mSE.get();
		mNeighbourSouth = TRY_GET_PTR(mParent->mNeighbourSouth, mNW.get());
		mNeighbourWest = TRY_GET_PTR(mParent->mNeighbourWest, mSE.get());
		break;
	case RQuadTreeTerrain::Type::ROOT:
	default:
		break;
	}

	if (mIsDivided)
	{
		mNW->UpdateNeigbours();
		mNE->UpdateNeigbours();
		mSW->UpdateNeigbours();
		mSE->UpdateNeigbours();
	}
}

ProTerGen::RQuadTreeTerrain::Border ProTerGen::RQuadTreeTerrain::GetBorder() const
{
	switch (mType)
	{
	case RQuadTreeTerrain::Type::NORTHWEST:
		if (mNeighbourNorth == nullptr && mNeighbourWest == nullptr)
		{
			return Border::NORTHWEST;
		}
		else if (mNeighbourNorth == nullptr)
		{
			return Border::NORTH;
		}
		else if (mNeighbourWest == nullptr)
		{
			return Border::WEST;
		}
		
		break;
	case RQuadTreeTerrain::Type::NORTHEAST:
		if (mNeighbourNorth == nullptr && mNeighbourEast == nullptr)
		{
			return Border::NORTHEAST;
		}
		else if (mNeighbourNorth == nullptr)
		{
			return Border::NORTH;
		}
		else if (mNeighbourEast == nullptr)
		{
			return Border::EAST;
		}

		break;
	case RQuadTreeTerrain::Type::SOUTHEAST:
		if (mNeighbourSouth == nullptr && mNeighbourEast == nullptr)
		{
			return Border::SOUTHEAST;
		}
		else if (mNeighbourSouth == nullptr)
		{
			return Border::SOUTH;
		}
		else if (mNeighbourEast == nullptr)
		{
			return Border::EAST;
		}
		
		break;
	case RQuadTreeTerrain::Type::SOUTHWEST:
		if (mNeighbourSouth == nullptr && mNeighbourWest == nullptr)
		{
			return Border::SOUTHWEST;
		}
		else if (mNeighbourSouth == nullptr)
		{
			return Border::SOUTH;
		}
		else if (mNeighbourWest == nullptr)
		{
			return Border::WEST;
		}
		
		break;
	case RQuadTreeTerrain::Type::ROOT:
	default:
		break;
	}
	return Border::NONE;
}

ProTerGen::RQuadTreeTerrain::Corner ProTerGen::RQuadTreeTerrain::GetCorner() const
{
	switch (mType)
	{
	case RQuadTreeTerrain::Type::NORTHWEST:
		if (mNeighbourNorth == nullptr || mNeighbourWest == nullptr) break;
		if (mNeighbourNorth->HasBorder(Border::WEST) && mNeighbourWest->HasBorder(Border::NORTH))
		{
			return Corner::NW;
		}
		break;
	case RQuadTreeTerrain::Type::NORTHEAST:
		if (mNeighbourNorth == nullptr || mNeighbourEast == nullptr) break;
		if (mNeighbourNorth->HasBorder(Border::EAST) && mNeighbourEast->HasBorder(Border::NORTH))
		{
			return Corner::NE;
		}
		break;
	case RQuadTreeTerrain::Type::SOUTHEAST:
		if (mNeighbourEast == nullptr || mNeighbourSouth == nullptr) break;
		if (mNeighbourEast->HasBorder(Border::SOUTH) && mNeighbourSouth->HasBorder(Border::EAST))
		{
			return Corner::SE;
		}
		break;
	case RQuadTreeTerrain::Type::SOUTHWEST:
		if (mNeighbourWest == nullptr || mNeighbourSouth == nullptr) break;
		if (mNeighbourWest->HasBorder(Border::SOUTH) && mNeighbourSouth->HasBorder(Border::WEST))
		{
			return Corner::SW;
		}
		break;
	case RQuadTreeTerrain::Type::ROOT:
	default:
		break;
	}
	return Corner::NONE;
}



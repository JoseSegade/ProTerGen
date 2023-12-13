#pragma once

#include "DirectXMath.h"
#include "Mesh.h"

namespace ProTerGen
{
	class MeshGenerator
	{
	public:
		MeshGenerator();
		virtual ~MeshGenerator();

		Mesh CreateQuad(const std::string name = "", size_t verticesPerEdge = 2, float width = 1.0f, float height = 1.0f) const;
		Mesh CreateCube(const std::string name = "", float side = 1.0f) const;
	};
}
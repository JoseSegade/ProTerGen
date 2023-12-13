#include "MeshGenerator.h"

ProTerGen::MeshGenerator::MeshGenerator()
{
}

ProTerGen::MeshGenerator::~MeshGenerator()
{
}

ProTerGen::Mesh ProTerGen::MeshGenerator::CreateQuad(std::string name, size_t verticesPerEdge, float width, float height) const
{
	verticesPerEdge = min(2, verticesPerEdge);

	const float offsetNorth = -height * 0.5f;
	const float offsetWest = -width * 0.5f;
	const float invVPE = 1.0f / (static_cast<float>(verticesPerEdge - 1));
	const float vertexSpacingNorth = height * invVPE;
	const float vertexSpacingWest = width * invVPE;

	Mesh mesh{};
	mesh.Name = name;
	mesh.Vertices.resize(verticesPerEdge * verticesPerEdge);

	for (size_t j = 0; j < verticesPerEdge; ++j)
	{
		for (size_t i = 0; i < verticesPerEdge; ++i)
		{
			const size_t idx = i + j * verticesPerEdge;

			const float posX = offsetWest + i * vertexSpacingWest;
			const float posZ = offsetNorth + j * vertexSpacingNorth;

			const float u = i / (static_cast<float>(verticesPerEdge) - 1.0);
			const float v = j / (static_cast<float>(verticesPerEdge) - 1.0);

			mesh.Vertices[idx] =
			{
				.Position = { posX, 0, posZ, 0.0f  },
				.Normal = { 0, 1, 0 },
				.TexC = { u, v },
				.TangentU = { 0, 0, 1}
			};
		}
	}

	for (size_t j = 0; j < verticesPerEdge - 1; ++j)
	{
		bool invert = j % 2 == 0;
		for (size_t i = 0; i < verticesPerEdge - 1; ++i)
		{
			uint32_t nw = static_cast<uint32_t>(i + j * verticesPerEdge);
			uint32_t ne = static_cast<uint32_t>((i + 1) + j * verticesPerEdge);
			uint32_t se = static_cast<uint32_t>((i + 1) + (j + 1) * verticesPerEdge);
			uint32_t sw = static_cast<uint32_t>(i + (j + 1) * verticesPerEdge);

			struct Tri
			{
				uint32_t a;
				uint32_t b;
				uint32_t c;
			};

			Tri tri1 = invert ? Tri{ nw, sw, se } : Tri{ nw, sw, ne };
			Tri tri2 = invert ? Tri{ nw, se, ne } : Tri{ sw, se, ne };

			mesh.Indices.push_back(tri1.c);
			mesh.Indices.push_back(tri1.b);
			mesh.Indices.push_back(tri1.a);

			mesh.Indices.push_back(tri2.c);
			mesh.Indices.push_back(tri2.b);
			mesh.Indices.push_back(tri2.a);

			invert = !invert;
		}
	}

	return mesh;
}

ProTerGen::Mesh ProTerGen::MeshGenerator::CreateCube(std::string name, float side) const
{
	const size_t faceCount = 6;
	const size_t perFaceVertex = 4;

	const float dc = side * 0.5f;

	Mesh mesh{};
	mesh.Name = name;
	mesh.Vertices.resize(faceCount * perFaceVertex);
	mesh.Indices.resize(36);

	mesh.Vertices[0] = { { -dc, -dc, -dc, 0.0f, }, { 0.0f, -1.0f, 0.0f }, { 0, 0 }, { -1.0f, 0.0f, 0.0f } };
	mesh.Vertices[1] = { { dc, -dc, -dc, 0.0f }, { 0.0f, -1.0f, 0.0f },  { 0, 1 }, { -1.0f, 0.0f, 0.0f } };
	mesh.Vertices[2] = { { -dc, -dc, dc, 0.0f  }, { 0.0f, -1.0f, 0.0f },  { 1, 0 }, { -1.0f, 0.0f, 0.0f } };
	mesh.Vertices[3] = { { dc, -dc, dc, 0.0f  }, { 0.0f, -1.0f, 0.0f },   { 1, 1 }, { -1.0f, 0.0f, 0.0f } };

	mesh.Vertices[4] = { { -dc, dc, dc, 0.0f }, { -1.0f, 0.0f, 0.0f },   { 0, 0 }, { 0.0f, 0.0f, -1.0f } };
	mesh.Vertices[5] = { { -dc, dc, -dc, 0.0f }, { -1.0f, 0.0f, 0.0f },  { 0, 1 }, { 0.0f, 0.0f, -1.0f } };
	mesh.Vertices[6] = { { -dc, -dc, dc, 0.0f }, { -1.0f, 0.0f, 0.0f },  { 1, 0 }, { 0.0f, 0.0f, -1.0f } };
	mesh.Vertices[7] = { { -dc, -dc, -dc, 0.0f }, { -1.0f, 0.0f, 0.0f }, { 1, 1 }, { 0.0f, 0.0f, -1.0f } };

	mesh.Vertices[8] = { { -dc, dc, -dc, 0.0f }, { 0.0f, 0.0f, -1.0f },   { 0, 0 }, { 1.0f, 0.0f, 0.0f } };
	mesh.Vertices[9] = { { dc, dc, -dc, 0.0f }, { 0.0f, 0.0f, -1.0f },    { 0, 1 }, { 1.0f, 0.0f, 0.0f } };
	mesh.Vertices[10] = { { -dc, -dc, -dc, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 1, 0 }, { 1.0f, 0.0f, 0.0f } };
	mesh.Vertices[11] = { { dc, -dc, -dc, 0.0f }, { 0.0f, 0.0f, -1.0f },  { 1, 1 }, { 1.0f, 0.0f, 0.0f } };

	mesh.Vertices[12] = { { dc, dc, -dc, 0.0f }, { 1.0f, 0.0f, 0.0f },  { 0, 0 }, { 0.0f, 0.0f, 1.0f } };
	mesh.Vertices[13] = { { dc, dc, dc, 0.0f }, { 1.0f, 0.0f, 0.0f },   { 0, 1 }, { 0.0f, 0.0f, 1.0f } };
	mesh.Vertices[14] = { { dc, -dc, -dc, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 1, 0 }, { 0.0f, 0.0f, 1.0f } };
	mesh.Vertices[15] = { { dc, -dc, dc, 0.0f }, { 1.0f, 0.0f, 0.0f },  { 1, 1 }, { 0.0f, 0.0f, 1.0f } };

	mesh.Vertices[16] = { { dc, dc, dc, 0.0f }, { 0.0f, 0.0f, 1.0f },   { 0, 0 }, { -1.0f, 0.0f, 0.0f } };
	mesh.Vertices[17] = { { -dc, dc, dc, 0.0f }, { 0.0f, 0.0f, 1.0f },  { 0, 1 }, { -1.0f, 0.0f, 0.0f } };
	mesh.Vertices[18] = { { dc, -dc, dc, 0.0f }, { 0.0f, 0.0f, 1.0f },  { 1, 0 }, { -1.0f, 0.0f, 0.0f } };
	mesh.Vertices[19] = { { -dc, -dc, dc, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1, 1 }, { -1.0f, 0.0f, 0.0f } };

	mesh.Vertices[20] = { { -dc, dc, dc, 0.0f }, { 0.0f, 1.0f, 0.0f },  { 0, 0 }, { 1.0f, 0.0f, 0.0f } };
	mesh.Vertices[21] = { { dc, dc, dc, 0.0f }, { 0.0f, 1.0f, 0.0f },   { 0, 1 }, { 1.0f, 0.0f, 0.0f } };
	mesh.Vertices[22] = { { -dc, dc, -dc, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 1, 0 }, { 1.0f, 0.0f, 0.0f } };
	mesh.Vertices[23] = { { dc, dc, -dc, 0.0f }, { 0.0f, 1.0f, 0.0f },  { 1, 1 }, { 1.0f, 0.0f, 0.0f } };

	for (size_t i = 0; i < faceCount; ++i)
	{
		size_t baseIdx = i * perFaceVertex;
		mesh.Indices[(i * faceCount) + 0] = static_cast<uint32_t>(baseIdx + 0);
		mesh.Indices[(i * faceCount) + 1] = static_cast<uint32_t>(baseIdx + 1);
		mesh.Indices[(i * faceCount) + 2] = static_cast<uint32_t>(baseIdx + 2);
		mesh.Indices[(i * faceCount) + 3] = static_cast<uint32_t>(baseIdx + 1);
		mesh.Indices[(i * faceCount) + 4] = static_cast<uint32_t>(baseIdx + 3);
		mesh.Indices[(i * faceCount) + 5] = static_cast<uint32_t>(baseIdx + 2);
	}

	return mesh;
}
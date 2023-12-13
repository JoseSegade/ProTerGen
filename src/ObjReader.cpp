#include "ObjReader.h"
#include <algorithm>
#include <fstream>
#include <sstream>

void ProcessFile(std::ifstream& file, ProTerGen::Mesh& m, bool& isDefinedByQuads)
{
	using namespace ProTerGen;

	std::stringstream ss;

	std::vector<DirectX::XMFLOAT4> vertexPositions;
	std::vector<DirectX::XMFLOAT3> vertexNormals;
	std::vector<DirectX::XMFLOAT2> vertexTexCoords;

	std::vector<Index> indicesPositions;
	std::vector<Index> indicesNormals;
	std::vector<Index> indicesTexCoords;

	std::string line   = "";
	std::string prefix = "";

	const char SLASH_SEPARATOR = '/';
	const char SPACE_SEPARATOR = ' ';
	const uint32_t TRIANGLE_VERTEX_COUNT = 3;
	const uint32_t QUAD_VERTEX_COUNT = 4;
	while (std::getline(file, line))
	{
		ss.clear();
		ss.str(line);
		ss >> prefix;
		if (prefix == "v")
		{
			DirectX::XMFLOAT4 v = { 0.0f, 0.0f, 0.0f, 1.0f };
			ss >> v.x >> v.y >> v.z;
			vertexPositions.push_back(v);
		}
		else if (prefix == "vt")
		{
			DirectX::XMFLOAT2 v = { 0.0f, 0.0f };
			ss >> v.x >> v.y;
			vertexTexCoords.push_back(v);
		}
		else if (prefix == "vn")
		{
			DirectX::XMFLOAT3 v = { 0.0f, 0.0f, 0.0f };
			ss >> v.x >> v.y >> v.z;
			vertexNormals.push_back(v);
		}
		else if (prefix == "f")
		{
			uint32_t temp     = 0;
			uint32_t counter  = 0;
			uint32_t sizeFace = 0;

			while (ss >> temp)
			{
				switch (counter)
				{
				case 0:
					indicesPositions.push_back(temp - 1);
					break;
				case 1:
					indicesTexCoords.push_back(temp - 1);
					break;
				case 2:
					indicesNormals.push_back(temp - 1);
					break;
				default:
					break;
				}

				if (ss.peek() == SLASH_SEPARATOR)
				{
					++counter;
					ss.ignore(1, SLASH_SEPARATOR);
				}
				else if (ss.peek() == SPACE_SEPARATOR)
				{
					++counter;
					++sizeFace;
					ss.ignore(1, SPACE_SEPARATOR);
				}

				counter = counter % TRIANGLE_VERTEX_COUNT;
			}
			
			// The + 1 to the size face is because it is incremented when a separator occurs.
			// When line end is reached, there is no additional separator.

			// This should be done only the first time, but checking if it is first time is rather inefficient.
			isDefinedByQuads = (sizeFace + 1) == QUAD_VERTEX_COUNT;

			// Process triangle
			if ((sizeFace + 1) == TRIANGLE_VERTEX_COUNT)
			{
				const uint32_t ip0 = indicesPositions[indicesPositions.size() - 3];
				const uint32_t ip1 = indicesPositions[indicesPositions.size() - 2];
				const uint32_t ip2 = indicesPositions[indicesPositions.size() - 1];

				const uint32_t iu0 = indicesTexCoords[indicesTexCoords.size() - 3];
				const uint32_t iu1 = indicesTexCoords[indicesTexCoords.size() - 2];
				const uint32_t iu2 = indicesTexCoords[indicesTexCoords.size() - 1];

				const uint32_t in0 = indicesNormals[indicesNormals.size() - 3];
				const uint32_t in1 = indicesNormals[indicesNormals.size() - 2];
				const uint32_t in2 = indicesNormals[indicesNormals.size() - 1];

				const DirectX::XMFLOAT4& p0 = vertexPositions[ip0];
				const DirectX::XMFLOAT4& p1 = vertexPositions[ip1];
				const DirectX::XMFLOAT4& p2 = vertexPositions[ip2];

				const DirectX::XMFLOAT2& u0 = vertexTexCoords[iu0];
				const DirectX::XMFLOAT2& u1 = vertexTexCoords[iu1];
				const DirectX::XMFLOAT2& u2 = vertexTexCoords[iu2];

				const DirectX::XMFLOAT3& n0 = vertexNormals[in0];
				const DirectX::XMFLOAT3& n1 = vertexNormals[in1];
				const DirectX::XMFLOAT3& n2 = vertexNormals[in2];

				const DirectX::XMFLOAT3 dp1 = { p1.x - p0.x, p1.y - p0.y, p1.z - p0.z };
				const DirectX::XMFLOAT3 dp2 = { p2.x - p0.x, p2.y - p0.y, p2.z - p0.z };

				const DirectX::XMFLOAT2 du1 = { u1.x - u0.x, u1.y - u0.y };
				const DirectX::XMFLOAT2 du2 = { u2.x - u0.x, u2.y - u0.y };

				const float r = 1.0f / (du1.x * du2.y - du1.y * du2.x);

				const DirectX::XMFLOAT3 t =
				{
					(dp1.x * du2.y - dp2.x * du1.y) * r,
					(dp1.y * du2.y - dp2.y * du1.y) * r,
					(dp1.z * du2.y - dp2.z * du1.y) * r,
				};

				// This method produces duplicated vertices. Beware.
				// Another aproximation is to make normals and tangents, 
				// checking for existing vertices.
				m.Vertices.push_back(Vertex
					{
						.Position = p0,
						.Normal   = n0,
						.TexC     = u0,
						.TangentU = t,
					});
				m.Vertices.push_back(Vertex
					{
						.Position = p1,
						.Normal   = n1,
						.TexC     = u1,
						.TangentU = t,
					});
				m.Vertices.push_back(Vertex
					{
						.Position = p2,
						.Normal   = n2,
						.TexC     = u2,
						.TangentU = t,
					});

				m.Indices.push_back(m.Vertices.size() - 3);
				m.Indices.push_back(m.Vertices.size() - 2);
				m.Indices.push_back(m.Vertices.size() - 1);
			}
		}
	}
}

bool ProTerGen::ObjReader::ReadObj(const std::string& path, Mesh& m, bool& isDefinedByQuads) const
{
	std::ifstream file(path);

	if (!file.is_open())
	{
		return false;
	}
	ProcessFile(file, m, isDefinedByQuads);
	file.close();

	return true;
}

bool ProTerGen::ObjReader::ReadObj(const std::wstring& path, Mesh& m, bool& isDefinedByQuads) const
{
	std::locale loc{};
	const wchar_t* wideptr = path.c_str();
	std::size_t len = std::wcslen(wideptr);
	std::vector<char> narrowPath(len);
	std::use_facet<std::ctype<wchar_t>>(loc).narrow(wideptr, wideptr + len, '?', &narrowPath[0]);
	narrowPath.push_back('\0');

	std::ifstream file(narrowPath.data());

	if (!file.is_open())
	{
		return false;
	}
	ProcessFile(file, m, isDefinedByQuads);
	file.close();

	return true;
}

#pragma once

#include "CommonHeaders.h"
#include "Mesh.h"

namespace ProTerGen
{
	class ObjReader
	{
	public:
		bool ReadObj(const std::string& path, Mesh& m, bool& isDefinedByQuads) const;
		bool ReadObj(const std::wstring& path, Mesh& m, bool& isDefinedByQuads) const;
	};
}

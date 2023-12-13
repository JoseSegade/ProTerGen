#pragma once

#include "CommonHeaders.h"

namespace ProTerGen
{
   class ShaderCompiler
   {
   public:
      ShaderCompiler() = default;
      virtual ~ShaderCompiler() = default;

      Microsoft::WRL::ComPtr<ID3DBlob> Compile
      (
         const std::wstring& filename,
         const std::vector<D3D_SHADER_MACRO>& defines,
         const std::string& entry,
         const std::string& target
      );
   };
}

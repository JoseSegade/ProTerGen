#include "ShaderCompiler.h"
#include "MathHelpers.h"

Microsoft::WRL::ComPtr<ID3DBlob> ProTerGen::ShaderCompiler::Compile
(
   const std::wstring& filename, 
   const std::vector<D3D_SHADER_MACRO>& defines, 
   const std::string& entry, 
   const std::string& target
)
{
	int32_t compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)  
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	Microsoft::WRL::ComPtr<ID3DBlob> byteCode = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errors = nullptr;

	HRESULT hr = D3DCompileFromFile
	(
		filename.c_str(), 
		defines.data(), 
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entry.c_str(), 
		target.c_str(), 
		compileFlags, 
		0, 
		byteCode.ReleaseAndGetAddressOf(),
		errors.ReleaseAndGetAddressOf()
	);

	if (errors != nullptr)
	{
		OutputDebugStringA(reinterpret_cast<char*>(errors->GetBufferPointer()));
		printf("%s, at entry %s:\n Error: %s\n", Wstring2String(filename).c_str(), entry.c_str(), (char*)(errors->GetBufferPointer()));
	}

	ThrowIfFailed(hr);

	return byteCode;
}

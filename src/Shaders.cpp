#include "Shaders.h"

Microsoft::WRL::ComPtr<ID3D12RootSignature> ProTerGen::Shaders::GetRootSignature(const std::string& name) const
{
	return mRootSignatures.at(name).RootSignature;
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> ProTerGen::Shaders::GetPSO(const std::string& name)
{
	return mPSOs.at(name);
}

void ProTerGen::Shaders::CreateRootSignature(const std::string& name)
{
	if (mRootSignatures.count(name) < 1)
	{
		mRootSignatures[name] = RootSignature{};
		mRootSignatures.at(name).LastShaderSpace.insert(std::make_pair(0, 0));
		mRootSignatures.at(name).LastTable.insert(std::make_pair(0, 0));
		mDirectXParamaters[name] = std::vector<CD3DX12_ROOT_PARAMETER1>{};
	}
}

void ProTerGen::Shaders::AddTableToRootSignature
(
	const std::string& tableName,
	const std::string& rootName,
	uint32_t elementNumber,
	D3D12_DESCRIPTOR_RANGE_TYPE descRange,
	D3D12_SHADER_VISIBILITY visibility,
	uint32_t slot
)
{
	CreateRootSignature(rootName);
	auto& rootSignature = mRootSignatures.at(rootName);
	auto& parameters = mDirectXParamaters.at(rootName);

	std::unique_ptr<CD3DX12_DESCRIPTOR_RANGE1> table = std::make_unique< CD3DX12_DESCRIPTOR_RANGE1>();
	table->Init(
		descRange,
		elementNumber,
		rootSignature.LastTable[slot],
		slot,
		D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
	);

	if (mDirectXRootTables.count(rootName) < 0)
	{
		mDirectXRootTables[rootName] = std::vector<std::unique_ptr<CD3DX12_DESCRIPTOR_RANGE1>>{};
	}

	auto& vectorDescriptorRange = mDirectXRootTables[rootName];
	vectorDescriptorRange.push_back(std::move(table));

	rootSignature.LastTable[slot] += elementNumber;

	parameters.push_back({});
	parameters.back().InitAsDescriptorTable(1, vectorDescriptorRange.back().get(), visibility);

	mRootParameters.insert(std::make_pair(tableName, RootParameter{ .RootSignature = rootName, .Location = static_cast<uint32_t>(parameters.size()) - 1, .Slot = slot }));
}

void ProTerGen::Shaders::AddConstantBufferToRootSignature(const std::string& bufferName, const std::string& rootName, uint32_t slot)
{
	CreateRootSignature(rootName);
	auto& rootSignature = mRootSignatures.at(rootName);
	auto& parameters = mDirectXParamaters.at(rootName);

	parameters.push_back({});
	parameters.back().InitAsConstantBufferView(rootSignature.LastShaderSpace[slot], slot);

	rootSignature.LastShaderSpace[slot]++;

	mRootParameters.insert(std::make_pair(bufferName, RootParameter{ .RootSignature = rootName, .Location = static_cast<uint32_t>(parameters.size()) - 1, .Slot = slot }));
}

void ProTerGen::Shaders::AddShaderResourceViewToRootSignature(const std::string& srvName, const std::string& rootName, uint32_t slot, D3D12_SHADER_VISIBILITY visibility)
{
	CreateRootSignature(rootName);
	auto& rootSignature = mRootSignatures.at(rootName);
	auto& parameters = mDirectXParamaters.at(rootName);

	parameters.push_back({});
	parameters.back().InitAsShaderResourceView(rootSignature.LastShaderSpace[slot], slot, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, visibility);

	rootSignature.LastShaderSpace[slot]++;

	mRootParameters.insert(std::make_pair(srvName, RootParameter{ .RootSignature = rootName, .Location = static_cast<uint32_t>(parameters.size()) - 1, .Slot = slot }));
}

void ProTerGen::Shaders::SetSamplersToRootSignature(const std::string& rootName, const std::vector<CD3DX12_STATIC_SAMPLER_DESC>& samplers)
{
	CreateRootSignature(rootName);
	mRootSignatures.at(rootName).Samplers = samplers;
}

void ProTerGen::Shaders::CompileRootSignatures(Microsoft::WRL::ComPtr<ID3D12Device> device)
{
	for (auto& [name, _] : mRootSignatures)
	{
		CompileRootSignature(device, name);
	}
}

void ProTerGen::Shaders::CompileRootSignature(Microsoft::WRL::ComPtr<ID3D12Device> device, const std::string& name)
{
	if (mRootSignatures.count(name) < 1) return;

	Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = {};

	auto& rootSignature = mRootSignatures[name];
	auto& parameters = mDirectXParamaters.at(name);
	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc
	(
		(uint32_t)parameters.size(),
		parameters.data(),
		static_cast<uint32_t>(rootSignature.Samplers.size()),
		rootSignature.Samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	hr = D3D12SerializeVersionedRootSignature
	(
		&rootSigDesc,
		serializedRootSig.ReleaseAndGetAddressOf(),
		errorBlob.ReleaseAndGetAddressOf()
	);

	if (errorBlob != nullptr)
	{
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		printf((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(device->CreateRootSignature
	(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(rootSignature.RootSignature.ReleaseAndGetAddressOf())
	));

	rootSignature.Samplers.clear();
	rootSignature.LastShaderSpace.clear();
	rootSignature.LastTable.clear();
	rootSignature.IsCompiled = true;

	mDirectXRootTables.erase(name);
	mDirectXParamaters.erase(name);
}

void ProTerGen::Shaders::CompileShaders
(
	const std::vector<std::string>& names,
	const std::vector<std::wstring>& path,
	const std::vector<TYPE>& types,
	const std::vector<D3D_SHADER_MACRO>& macroDefines
)
{
	assert(names.size() == path.size() && names.size() == types.size());
	ShaderCompiler sc = ShaderCompiler();
	for (size_t i = 0; i < names.size(); ++i)
	{
		std::string entry = "";
		std::string order = "";
		switch (types.at(i))
		{
		case VS:
			entry = "VS";
			order = "vs_5_1";
			break;
		case PS:
			entry = "PS";
			order = "ps_5_1";
			break;
		case GS:
			entry = "GS";
			order = "gs_5_1";
			break;
		case HS:
			entry = "HS";
			order = "hs_5_1";
			break;
		case DS:
			entry = "DS";
			order = "ds_5_1";
			break;
		case CS:
			entry = "CS";
			order = "cs_5_1";
			break;
		default:
			break;
		}

		mShaders[names.at(i)] = { .Blob = sc.Compile(path.at(i), macroDefines, entry, order), .Type = types.at(i) };
	}
}

std::vector<D3D12_INPUT_ELEMENT_DESC>& ProTerGen::Shaders::InputLayout(const std::string& name)
{
	return mInputLayouts[name];
}

std::pair<uint32_t, uint32_t> ProTerGen::Shaders::GetLocation(std::string name) const
{
	RootParameter rp = mRootParameters.at(name);
	return std::make_pair(rp.Location, rp.Slot);
}

void ProTerGen::Shaders::CreateGraphicPSO
(
	const std::string& name,
	Microsoft::WRL::ComPtr<ID3D12Device> device,
	const std::string& rootName,
	const std::string& inputLayoutName,
	const std::vector<std::string>& shaderNames,
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc
)
{
	assert(mRootSignatures.at(rootName).IsCompiled);

	desc.pRootSignature = mRootSignatures.at(rootName).RootSignature.Get();
	desc.InputLayout = { mInputLayouts.at(inputLayoutName).data(), static_cast<uint32_t>(mInputLayouts.at(inputLayoutName).size()) };

	for (const std::string& name : shaderNames)
	{
		Shader& shader = mShaders.at(name);
		switch (shader.Type)
		{
		case VS:
			desc.VS =
			{
			   .pShaderBytecode = reinterpret_cast<BYTE*>(shader.Blob->GetBufferPointer()),
			   .BytecodeLength = shader.Blob->GetBufferSize()
			};
			break;
		case PS:
			desc.PS =
			{
			   .pShaderBytecode = reinterpret_cast<BYTE*>(shader.Blob->GetBufferPointer()),
			   .BytecodeLength = shader.Blob->GetBufferSize()
			};
			break;
		case GS:
			desc.GS =
			{
			   .pShaderBytecode = reinterpret_cast<BYTE*>(shader.Blob->GetBufferPointer()),
			   .BytecodeLength = shader.Blob->GetBufferSize()
			};
			break;
		case HS:
			desc.HS =
			{
			   .pShaderBytecode = reinterpret_cast<BYTE*>(shader.Blob->GetBufferPointer()),
			   .BytecodeLength = shader.Blob->GetBufferSize()
			};
			break;
		case DS:
			desc.DS =
			{
			   .pShaderBytecode = reinterpret_cast<BYTE*>(shader.Blob->GetBufferPointer()),
			   .BytecodeLength = shader.Blob->GetBufferSize()
			};
			break;
		default:
			break;
		}
	}

	ThrowIfFailed(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(mPSOs[name].ReleaseAndGetAddressOf())));
}

void ProTerGen::Shaders::CreateComputePSO
(
	const std::string& name,
	Microsoft::WRL::ComPtr<ID3D12Device> device,
	const std::string& rootName,
	const std::string& shaderName
)
{
	assert(mRootSignatures.at(rootName).IsCompiled);

	Shader& shader = mShaders.at(shaderName);

	assert(shader.Type == TYPE::CS);


	D3D12_COMPUTE_PIPELINE_STATE_DESC cPSODesc =
	{
		.pRootSignature = mRootSignatures.at(rootName).RootSignature.Get(),
		.CS =
		{
			.pShaderBytecode = reinterpret_cast<BYTE*>(shader.Blob->GetBufferPointer()),
			.BytecodeLength = shader.Blob->GetBufferSize()
		}
	};

	ThrowIfFailed(device->CreateComputePipelineState(&cPSODesc, IID_PPV_ARGS(mPSOs[name].ReleaseAndGetAddressOf())));
}

void ProTerGen::Shaders::FreeShadersMemory()
{
	mShaders.clear();
	mInputLayouts.clear();
}

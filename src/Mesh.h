#pragma once

#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <unordered_map>
#include "CommonHeaders.h"

namespace ProTerGen
{
	using Index = uint32_t;

	struct Vertex
	{
		DirectX::XMFLOAT4 Position = { 0.0f, 0.0f, 0.0f, 0.0f }; // Last float corresponds to lodlevel
		DirectX::XMFLOAT3 Normal   = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT2 TexC     = { 0.0f, 0.0f };
		DirectX::XMFLOAT3 TangentU = { 0.0f, 0.0f, 0.0f };
	};

	struct Mesh
	{
		std::string Name = "";

		std::vector<Vertex> Vertices{};
		std::vector<Index> Indices{};
	};

	struct SubmeshParameters
	{
		size_t IndexCount           = 0;
		size_t StartIndexLocation   = 0;
		size_t BaseVertexLocation   = 0;
		size_t BaseInstanceLocation = 0;
		size_t NumInstances         = 1;
	};

	struct MeshGpu
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU        = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU         = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> InstanceBufferGPU      = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader   = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader    = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> InstanceBufferUploader = nullptr;

		size_t VertexByteStride       = 0;
		size_t VertexBufferByteSize   = 0;
		DXGI_FORMAT IndexFormat       = DXGI_FORMAT_R32_UINT;
		size_t IndexBufferByteSize    = 0;
		size_t InstanceBufferStride   = 0;
		size_t InstanceBufferByteSize = 0;

		std::unordered_map<std::string, SubmeshParameters> SubMesh{};

		D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const
		{
			D3D12_VERTEX_BUFFER_VIEW vbv =
			{
				.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress(),
				.SizeInBytes    = static_cast<UINT>(VertexBufferByteSize),
				.StrideInBytes  = static_cast<UINT>(VertexByteStride),
			};
			return vbv;
		}

		D3D12_INDEX_BUFFER_VIEW IndexBufferView() const
		{
			D3D12_INDEX_BUFFER_VIEW ibv =
			{
				.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress(),
				.SizeInBytes    = static_cast<UINT>(IndexBufferByteSize),
				.Format         = IndexFormat
			};
			return ibv;
		}

		D3D12_VERTEX_BUFFER_VIEW InstanceBufferView() const
		{
			if (InstanceBufferGPU == nullptr) return {};
			D3D12_VERTEX_BUFFER_VIEW xbv =
			{
				.BufferLocation = InstanceBufferGPU->GetGPUVirtualAddress(),
				.SizeInBytes    = static_cast<UINT>(InstanceBufferByteSize),
				.StrideInBytes  = static_cast<UINT>(InstanceBufferStride),
			};
			return xbv;
		}

		void DisposeUploaders()
		{
			VertexBufferUploader   = nullptr;
			IndexBufferUploader    = nullptr;
			InstanceBufferUploader = nullptr;
		}
	};	
}

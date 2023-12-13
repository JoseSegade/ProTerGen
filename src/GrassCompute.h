#pragma once

#include "CommonHeaders.h"
#include <DirectXMath.h>
#include "Shaders.h"
#include "DescriptorHeaps.h"
#include "UploadBuffer.h"

namespace ProTerGen
{
	struct GrassComputeSettings
	{
		float radius = 1.0f;
		float maxNumber = 10 * 1024 * 1024;
	};
	class GrassCompute
	{
	public:
		GrassCompute(){}
		~GrassCompute() {};

		void Init
		(
			Microsoft::WRL::ComPtr<ID3D12Device> device,
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
			Shaders& shaders,
			DescriptorHeaps& descriptorHeap,
			GrassComputeSettings settings
		);
		void Compute
		(
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, 
			Shaders& shaders, 
			DescriptorHeaps& descriptorHeaps,
			const DirectX::XMFLOAT3& eyePos
		);
	private:
		struct GrassCB
		{
			DirectX::XMFLOAT3 eyePos;
			float distance;
			uint32_t grassPoolSize;
			DirectX::XMFLOAT3 __pad;
		};

		GrassComputeSettings mSettings = {};

		Microsoft::WRL::ComPtr<ID3D12Resource> mPool   = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> mPoolUp = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> mGrass  = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> mSize   = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> mSizeRB = nullptr;

		std::unique_ptr<UploadBuffer<GrassCB>> mGrassCB = nullptr;

		uint32_t mPoolIdx = 0;
		uint32_t mGrassIdx = 0;
		uint32_t mSizeIdx = 0;
	};
}

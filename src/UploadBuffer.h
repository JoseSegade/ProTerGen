#pragma once

#include <stdio.h>
#include "CommonHeaders.h"
#include "MathHelpers.h"

namespace ProTerGen
{
	class GenericUploadBuffer
	{
	public:
		GenericUploadBuffer() = default;
		virtual ~GenericUploadBuffer() = default;
	};

	template<typename T>
	class UploadBuffer : public GenericUploadBuffer
	{
	public:
		explicit UploadBuffer(Microsoft::WRL::ComPtr<ID3D12Device> device, size_t elementCount, bool isConstant)
			: mElementByteSize(sizeof(T)), mIsConstant(isConstant)
		{
			if (isConstant)
			{
				mElementByteSize = (uint32_t)Align(sizeof(T), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);
			}

			Resize(device, elementCount);
		};

		virtual ~UploadBuffer() override
		{
			if (mUploadBuffer != nullptr)
			{
				mUploadBuffer->Unmap(0, nullptr);
			}
			mMappedData = nullptr;
		};

		UploadBuffer(const UploadBuffer&) = delete;
		UploadBuffer& operator=(const UploadBuffer&) = delete;

		void Resize(Microsoft::WRL::ComPtr<ID3D12Device> device, size_t elementCount)
		{
			if (mUploadBuffer != nullptr)
			{
				mUploadBuffer->Unmap(0, nullptr);
			}
			mMappedData = nullptr;

			const CD3DX12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			const CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(mElementByteSize * elementCount);
			ThrowIfFailed(device->CreateCommittedResource
			(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(mUploadBuffer.ReleaseAndGetAddressOf())
			));

			ThrowIfFailed(mUploadBuffer->Map(0, nullptr, (void**)(&mMappedData)));
		}

		Microsoft::WRL::ComPtr<ID3D12Resource> Resource() const
		{
			return mUploadBuffer;
		}

		void CopyData(uint32_t offset, const T& data)
		{
			size_t initialIndex = static_cast<size_t>(offset) * mElementByteSize;
			size_t size = sizeof(T);
			memcpy(&mMappedData[initialIndex], &data, size);
		}

	protected:
		Microsoft::WRL::ComPtr<ID3D12Resource> mUploadBuffer;
		BYTE* mMappedData = nullptr;
		uint32_t mElementByteSize;
		bool mIsConstant;
	};
}

#pragma once

#include "CommonHeaders.h"
#include <thread>
#include <functional>
#include <queue>

#include "Texture.h"
#include "UploadBuffer.h"

namespace ProTerGen
{
	struct GpuComputePipeline
	{
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandAllocator = nullptr;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> CommandList = nullptr;

		std::vector<Microsoft::WRL::ComPtr<ID3D12PipelineState>> PSOs;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSignature = nullptr;

		std::vector<std::string> HeapCollections;
		std::vector<Texture*> Textures;
		std::vector<GenericUploadBuffer*> Buffers;

		~GpuComputePipeline()
		{
			for (size_t i = 0; i < Buffers.size(); ++i)
			{
				delete Buffers[i];
			}
			Buffers.clear();
		}
	};

	class GpuBatches
	{
	public:

		void Init(Microsoft::WRL::ComPtr<ID3D12Device> device);
		uint32_t CreateGpuComputePipeline(Microsoft::WRL::ComPtr<ID3D12Device> device);
		GpuComputePipeline& GetGpuComputePipeline(uint32_t id);
		void StopBatch() {};

		void ExecuteGpuPipeline(uint32_t index);
		void ExecuteGpuPipeline(GpuComputePipeline& gpuComputePipeline);
		void ExecuteGpuPipelines(const std::vector<uint32_t> index);

		void WaitForGpuCompletion();

	private:
		void Signal();

		Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Fence> mFence = nullptr;
		std::atomic<size_t> mCurrentFence = 0;

		using cp_index = uint32_t;

		std::vector<GpuComputePipeline> mComputePipelines;
		std::queue<cp_index> mFreeComputePipelines;
	};
}
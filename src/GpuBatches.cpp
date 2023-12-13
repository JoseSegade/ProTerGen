#include "GpuBatches.h"

void ProTerGen::GpuBatches::Init(Microsoft::WRL::ComPtr<ID3D12Device> device)
{
	D3D12_COMMAND_QUEUE_DESC queueDesc =
	{
		.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE,
		.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE
	};

	ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));
	mCommandQueue->SetName(L"Batch_Queue");

	ThrowIfFailed(device->CreateFence
	(
		0,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(mFence.ReleaseAndGetAddressOf())
	));
	mFence->SetName(L"Batch_Fence");
}

uint32_t ProTerGen::GpuBatches::CreateGpuComputePipeline(Microsoft::WRL::ComPtr<ID3D12Device> device)
{
	uint32_t index = (uint32_t)(mComputePipelines.size());
	if (mFreeComputePipelines.empty())
	{
		mComputePipelines.push_back({});
		GpuComputePipeline& gpuComputePipeline = mComputePipelines[index];

		ThrowIfFailed(device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_COMPUTE,
			IID_PPV_ARGS(gpuComputePipeline.CommandAllocator.ReleaseAndGetAddressOf())));
		gpuComputePipeline.CommandAllocator->SetName((L"Batch_Alloc_" + std::to_wstring(index)).c_str());

		ThrowIfFailed(device->CreateCommandList
		(
			0,
			D3D12_COMMAND_LIST_TYPE_COMPUTE,
			gpuComputePipeline.CommandAllocator.Get(),
			nullptr,
			IID_PPV_ARGS(gpuComputePipeline.CommandList.ReleaseAndGetAddressOf())
		));
		gpuComputePipeline.CommandList->SetName((L"Batch_List_" + std::to_wstring(index)).c_str());
	}
	else
	{
		index = mFreeComputePipelines.front();
		mFreeComputePipelines.pop();
	}	

	return index;
}

ProTerGen::GpuComputePipeline& ProTerGen::GpuBatches::GetGpuComputePipeline(uint32_t id)
{
	return mComputePipelines[id];
}

void ProTerGen::GpuBatches::ExecuteGpuPipeline(uint32_t index)
{
	GpuComputePipeline& computePipeline = mComputePipelines[index];
	ExecuteGpuPipeline(computePipeline);
}

void ProTerGen::GpuBatches::ExecuteGpuPipeline(GpuComputePipeline& computePipeline)
{
	ThrowIfFailed(computePipeline.CommandList->Close());
	ID3D12CommandList* cmdsLists[] = { computePipeline.CommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	Signal();
}

void ProTerGen::GpuBatches::ExecuteGpuPipelines(const std::vector<uint32_t> index)
{
	const uint32_t size = (uint32_t)index.size();
	std::vector<ID3D12CommandList*> commandLists(size);
	for (uint32_t i = 0; i < size; ++i)
	{
		const uint32_t idx = index[i];
		if (idx < mComputePipelines.size())
		{
			commandLists[i] = mComputePipelines[idx].CommandList.Get();
		}
	}
	mCommandQueue->ExecuteCommandLists(commandLists.size(), commandLists.data());

	Signal();
}

void ProTerGen::GpuBatches::WaitForGpuCompletion()
{
	auto completedValue = mFence->GetCompletedValue();
	if (mFence->GetCompletedValue() < mCurrentFence)
	{
		HANDLE eventHandle = CreateEvent(NULL, FALSE, FALSE, NULL);

		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));
		assert(eventHandle != 0);

		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

void ProTerGen::GpuBatches::Signal()
{
	++mCurrentFence;
	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));
}

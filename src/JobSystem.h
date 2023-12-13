/// This code is obtained from WickedEngine
/// original author: turanszkij
/// https://wickedengine.net/2018/11/24/simple-job-system-using-standard-c/
/// https://github.com/turanszkij/WickedEngine.git

#pragma once

#include "CommonHeaders.h"

#include <functional>
#include <atomic>

namespace ProTerGen::JobSystem
{
	struct JobDesc
	{
		uint32_t JobIndex;
		uint32_t GroupId;
		uint32_t GroupIndex;
		bool IsFirstInGroup;
		bool IsLastInGroup;
		void* SharedMemory;
	};

	struct Context
	{
		std::atomic<uint32_t> counter{ 0 };
	};

	void Initialize(uint32_t maxThreadCount = ~0u);
	void Execute(Context& ctx, const std::function<void(JobDesc)>& task);

	void Dispatch(Context& ctx, uint32_t jobCount, uint32_t groupSize, const std::function<void(JobDesc)>& task, size_t sharedMemorySize = 0);

	uint32_t DispatchGroupCount(uint32_t jobCount, uint32_t groupSize);

	bool IsBusy(const Context& ctx);

	void Wait(const Context& ctx);
}
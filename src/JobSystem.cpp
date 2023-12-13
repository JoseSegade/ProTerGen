#include "JobSystem.h"
#include "ConcurrentQueue.h"

#include <mutex>

namespace ProTerGen::JobSystem
{
	struct Job
	{
		std::function<void(JobDesc)> Task;
		Context* Ctx;
		uint32_t GroupId;
		uint32_t GroupJobOffset;
		uint32_t GroupJobEnd;
		uint32_t SharedMemorySize;
	};

	typedef BConcurrentQueue<Job> JobQueue;

	struct InternalState
	{
		uint32_t NumCores = 0;
		uint32_t NumThreads = 0;
		std::unique_ptr<JobQueue[]> JobQueuePerThread;
		std::atomic_bool Alive{ true };
		std::condition_variable WakeCondition;
		std::mutex WakeMutex;
		std::atomic<uint32_t> NextQueue{ 0 };
		std::vector<std::thread> Threads;

		~InternalState()
		{
			Alive.store(false);
			bool wakeLoop = true;
			std::thread waker([&] {
				while (wakeLoop)
				{
					WakeCondition.notify_all();
				}
				});

			for (auto& thread : Threads)
			{
				thread.join();
			}

			wakeLoop = false;
			waker.join();
		}
	} static sInternalState;

	void Work(uint32_t startingQueue)
	{
		Job job = {};
		for (uint32_t i = 0; i < sInternalState.NumThreads; ++i)
		{
			JobQueue& jobQueue = sInternalState.JobQueuePerThread[startingQueue % sInternalState.NumThreads];
			while (jobQueue.TryDequeue(job))
			{
				JobDesc jobDesc = {};
				jobDesc.GroupId = job.GroupId;
				
				if (job.SharedMemorySize > 0)
				{
					thread_local static std::vector<uint8_t> sharedAllocationData;
					sharedAllocationData.reserve(job.SharedMemorySize);
					jobDesc.SharedMemory = sharedAllocationData.data();
				}
				else
				{
					jobDesc.SharedMemory = nullptr;
				}

				for (uint32_t i = job.GroupJobOffset; i < job.GroupJobEnd; ++i)
				{
					jobDesc.JobIndex = i;
					jobDesc.GroupIndex = i - job.GroupJobOffset;
					jobDesc.IsFirstInGroup = (i == job.GroupJobOffset);
					jobDesc.IsLastInGroup = (i == job.GroupJobEnd - 1);
					job.Task(jobDesc);
				}

				if (job.Ctx != nullptr)
				{
					job.Ctx->counter.fetch_sub(1);
				}
			}
			++startingQueue;
		}

	}

	void Initialize(uint32_t maxThreadCount)
	{
		if (sInternalState.NumThreads > 0)
		{
			return;
		}
		maxThreadCount = max(1u, maxThreadCount);

		sInternalState.NumCores = std::thread::hardware_concurrency();

		sInternalState.NumThreads = min(maxThreadCount, max(1u, sInternalState.NumCores - 1));
		sInternalState.JobQueuePerThread.reset(new JobQueue[sInternalState.NumThreads]);
		sInternalState.Threads.reserve(sInternalState.NumThreads);

		for (uint32_t threadId = 0; threadId < sInternalState.NumThreads; ++threadId)
		{
			sInternalState.Threads.emplace_back([threadId]
				{
					while (sInternalState.Alive.load())
					{
						Work(threadId);
						std::unique_lock<std::mutex> lock(sInternalState.WakeMutex);
						sInternalState.WakeCondition.wait(lock);
					}
				});

			std::thread& worker = sInternalState.Threads.back();

			HANDLE handle = (HANDLE)worker.native_handle();

			DWORD_PTR affinityMask = 1ull << threadId;
			DWORD_PTR affinityResult = SetThreadAffinityMask(handle, affinityMask);
			assert(affinityResult > 0);

			std::wstring threadName = L"JobThread_" + std::to_wstring(threadId);
			assert(SUCCEEDED(SetThreadDescription(handle, threadName.c_str())));
		}
	}

	void Execute(Context& ctx, const std::function<void(JobDesc)>& task)
	{
		ctx.counter.fetch_add(1);

		Job job =
		{
			.Task = task,
			.Ctx = &ctx,
			.GroupId = 0,
			.GroupJobOffset = 0,
			.GroupJobEnd = 0,
			.SharedMemorySize = 0
		};

		sInternalState.JobQueuePerThread[sInternalState.NextQueue.fetch_add(1) % sInternalState.NumThreads].Enqueue(job);
		sInternalState.WakeCondition.notify_one();
	}

	void Dispatch(Context& ctx, uint32_t jobCount, uint32_t groupSize, const std::function<void(JobDesc)>& task, size_t sharedMemorySize)
	{
		if (jobCount == 0) return;
		if (groupSize == 0) return;

		const uint32_t groupCount = DispatchGroupCount(jobCount, groupSize);

		ctx.counter.fetch_add(groupCount);

		Job job =
		{
			.Task = task,
			.Ctx = &ctx,
			.SharedMemorySize = static_cast<uint32_t>(sharedMemorySize)
		};

		for (uint32_t groupId = 0; groupId < groupCount; ++groupId)
		{
			job.GroupId = groupId;
			job.GroupJobOffset = groupId * groupSize;
			job.GroupJobEnd = min(job.GroupJobOffset + groupSize, jobCount);

			sInternalState.JobQueuePerThread[sInternalState.NextQueue.fetch_add(1) % sInternalState.NumThreads].Enqueue(job);
		}

		sInternalState.WakeCondition.notify_all();
	}

	uint32_t DispatchGroupCount(uint32_t jobCount, uint32_t groupSize)
	{
		return (jobCount + groupSize - 1) / groupSize;
	}

	bool IsBusy(const Context& ctx)
	{
		return ctx.counter.load() > 0;
	}

	void Wait(const Context& ctx)
	{
		if (IsBusy(ctx))
		{
			sInternalState.WakeCondition.notify_all();

			Work(sInternalState.NextQueue.fetch_add(1) % sInternalState.NumThreads);

			while (IsBusy(ctx))
			{
				std::this_thread::yield();
			}
		}
	}
}

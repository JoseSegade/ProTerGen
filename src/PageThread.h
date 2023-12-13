#pragma once

#include <functional>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <memory>

#include "ConcurrentQueue.h"

namespace ProTerGen
{
	namespace VT
	{
		// Creates a doble queue for page request adminsitration -> One is for pending requests and the other for the completed ones. 
		// This class is used in the tiled file reading for obtaining pages. It processes the requests asyncronously.
		template<typename T>
		class PageThread
		{
		public:
			~PageThread() noexcept
			{
				Dispose();
			}

			void OnRun(const std::function<bool(T&)>& function) { mOnRun = function; }
			void OnComplete(const std::function<void(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>, const T&)>& function) { mOnComplete = function; }

			void MaxQueueSize(size_t newSize)
			{
				mActionQueue.SetMaxSize(newSize);
				mCompleteQueue.SetMaxSize(newSize);
			}

			void Init()
			{
				mIsRunning.store(true);
				mThread = std::jthread(&ProTerGen::VT::PageThread<T>::Execute, this);
			}

			void Execute()
			{
				while (mIsRunning.load())
				{
					std::unique_lock<std::mutex> ul(mMutex);
					if (mSemaphore.load() == 0)
					{
						mCond.wait(ul);
					}
					if (!mIsRunning.load()) return;

					T element = {};
					if (!mActionQueue.TryDequeue(element))
					{
						mSemaphore.fetch_add(-1);
						mCond.notify_one();
						continue;
					}

					mOnRun(element);
					mCompleteQueue.Enqueue(element);
					mSemaphore.fetch_add(-1);
				}
			}

			void Update(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, uint32_t count)
			{
				for (uint32_t i = 0; i < count && !mCompleteQueue.IsEmpty(); ++i)
				{
					T element = {};
					if (!mCompleteQueue.TryDequeue(element))
					{
						break;
					}
					mOnComplete(commandList, element);
				}
			}

			void Enqueue(T& value)
			{
				if (mActionQueue.Enqueue(value))
				{
					mSemaphore.fetch_add(1);
					mCond.notify_one();
				}
			}

			void Dispose() noexcept
			{
				mIsRunning.store(false);
				mCond.notify_all();
				if (mThread.joinable())
				{
					mThread.join();
				}
			}

		private:
			std::function<void(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>, const T&)> mOnComplete
				= [](Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, const T&) {};
			std::function<bool(T&)> mOnRun = [](T&) { return true; };

			std::condition_variable mCond;
			std::jthread mThread;
			std::atomic_bool mIsRunning = false;
			std::atomic_int mSemaphore = 0;
			std::mutex mMutex;

			NBConcurrentQueue<T> mActionQueue;
			NBConcurrentQueue<T> mCompleteQueue;
		};
	}
}

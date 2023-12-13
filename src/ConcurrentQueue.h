#pragma once

#include <mutex>
#include <deque>
#include "CommonHeaders.h"

namespace ProTerGen
{
	template<typename T>
	class BConcurrentQueue
	{
	public:
		~BConcurrentQueue() { mQueue.clear(); }

		inline bool IsEmpty() { return mQueue.size() == 0; }
		inline size_t volatile Size() { return mQueue.size(); }

		inline void Enqueue(const T& value)
		{
			std::scoped_lock lock(mLocker);
			T val;
			val = value;
			mQueue.push_back(val);
		}

		inline bool TryDequeue(T& value)
		{
			std::scoped_lock lock(mLocker);
			if (mQueue.empty())
			{
				return false;
			}
			value = mQueue.front();
			mQueue.pop_front();
			return true;
		}

	private:
		std::deque<T> mQueue;
		std::mutex mLocker;
	};

	template<typename T>
	class NBConcurrentQueue
	{
	public:
		explicit NBConcurrentQueue() { mHead = new Node(); mTail = mHead; }
		~NBConcurrentQueue()
		{
			delete mHead;
			mHead = nullptr;
			mTail = nullptr;
		}

		inline bool IsEmpty() { return mHead == mTail; }
		inline size_t volatile Size() { return mSize.load(); }
		inline void SetMaxSize(size_t max) { mMaxSize.store(max); }

		inline bool Enqueue(T& value)
		{
			if (mSize.load() == mMaxSize.load()) return false;

			Node* node = new Node();
			node->value = value;

			Node* volatile oldTail;
			Node* volatile oldNext;
			while (true)
			{
				oldTail = mTail;
				oldNext = (mTail) ? (mTail->next) : nullptr;

				if (oldTail == mTail)
				{
					if (!oldNext)
					{
						void** _next = reinterpret_cast<void**>(&(mTail->next));
						if (!InterlockedCompareExchangePointer(_next, node, nullptr))
							break;
					}
					void** t = reinterpret_cast<void**>(&mTail);
					InterlockedCompareExchangePointer(t, oldNext, oldTail);
				}
			}
			void** t = reinterpret_cast<void**>(&mTail);
			InterlockedCompareExchangePointer(t, node, oldTail);
			mSize.fetch_add(1);
			return true;
		}

		inline bool TryDequeue(T& value)
		{
			Node* volatile oldHead;
			Node* volatile oldNext;
			Node* volatile oldTail;

			while (true)
			{
				oldHead = mHead;
				oldTail = mTail;
				oldNext = (mHead) ? (mHead->next) : nullptr;

				if (oldHead == mHead)
				{
					if (oldHead == oldTail)
					{
						return false;
					}
					else
					{
						value = oldNext->value;
						void** h = reinterpret_cast<void**>(&mHead);
						auto ptr = InterlockedCompareExchangePointer(h, oldNext, oldHead);
						if (ptr == oldHead)
						{
							delete ptr;
							break;
						}
					}
				}
			}

			mSize.fetch_sub(1);
			return true;
		}

	private:
		struct Node
		{
			T value;
			Node* next = nullptr;

			~Node()
			{
				if (next)
				{
					delete next;
					next = nullptr;
				}
			}

		};

		std::atomic<size_t> mSize = 0;
		std::atomic<size_t> mMaxSize = 0;

		Node* mHead = nullptr;
		Node* mTail = nullptr;
	};
}
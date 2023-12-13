#pragma once

#include <list>
#include <unordered_map>
#include <functional>

namespace ProTerGen
{
	template<typename key_t, typename value_t>
	class LRUCache
	{
	private:
		typedef std::pair<key_t, value_t> Item;
		typedef std::list<Item> ItemList;
		typedef ItemList::iterator ItemListIt;
		typedef std::unordered_map<key_t, ItemListIt>::iterator MapIt;

	public:
		inline void OnRemove(const std::function<void(key_t, value_t)>& func)
		{
			mOnRemove = func;
		}

		inline void Resize(size_t newSize)
		{
			mCapacity = newSize;
			Clear();
		}

		inline size_t size()
		{
			return mMap.size();
		}

		void Add(const key_t& key, const value_t& value)
		{
			MapIt it = mMap.find(key);
			if (it != mMap.end())
			{
				mList.erase(it->second);
				mMap.erase(it);
			}

			mList.push_front(std::make_pair(key, value));
			mMap.insert(std::make_pair(key, mList.begin()));
			Clear();
		}

		const std::list<Item> Items() const
		{
			return mList;
		}

		bool TryGet(const key_t& key, value_t& value, bool update)
		{
			MapIt it = mMap.find(key);
			if (it == mMap.end()) return false;
			if (update)
			{
				mList.splice(mList.begin(), mList, it->second);
			}
			value = it->second->second;
			return true;
		}

		value_t RemoveLast()
		{
			return RemoveLastPair().second;
		}

		inline bool ContainsKey(const key_t& key)
		{
			return mMap.count(key) > 0;
		}

	private:
		Item RemoveLastPair()
		{
			ItemListIt it = mList.end();
			it--;
			Item ret = std::make_pair(it->first, it->second);
			mMap.erase(it->first);
			mList.pop_back();
			mOnRemove(ret.first, ret.second);
			return ret;
		}

		void Clear()
		{
			while (mMap.size() > mCapacity)
			{
				RemoveLastPair();
			}
		}

	private:

		size_t mCapacity = 0;
		ItemList mList;
		std::unordered_map<key_t, ItemListIt> mMap;

		std::function<void(key_t, value_t)> mOnRemove = [](key_t, value_t) {};
	};
}

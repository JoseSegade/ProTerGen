#pragma once

#include "VirtualTextureCommon.h"
#include "PageThread.h"

namespace ProTerGen
{
	namespace VT
	{		
		// Represents a tiled disk file containing virtual texture with the order described in a previously specified indexer
		class TileDataFile
		{
		public:
			enum AccessMode : uint32_t
			{
				_NULL = 0,
				WRITE = 1 << 0,
				READ = 1 << 1,
				READ_WRITE = WRITE | READ
			};

			~TileDataFile();

			void Open(const std::wstring& filename, const VTDesc* info, uint32_t formatSize, AccessMode accessMode);
			void Open(const std::string& filename, const VTDesc* info, uint32_t formatSize, AccessMode accessMode);
			void Close();
			void WritePage(PageIndex index, const data_ptr data);
			void WriteCharOnBeginning(char c);
			bool ReadPage(PageIndex index, data_ptr data, uint32_t formatSize) const;
			bool ReadTile(PageIndex index, data_ptr data) const;
		private:
			const VTDesc* mInfo = nullptr;
			uint32_t mFormatSize = 0;
			mutable FILE* mFile = nullptr;
			AccessMode mMode = AccessMode::_NULL;
		};

		// Loads tile format files from disk in a different thread.
		class PageLoaderFromDisk
		{
		public:
			~PageLoaderFromDisk();

			inline void OnLoadComplete(const std::function<void(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>, const Page&, const data_ptr&)>& func) { mOnLoadComplete = func; }

			inline void EnableShowBorders(bool value) { mShowBorders = value; }
			inline void BordersColor(float color[4]) { *((float*)&mBorderColor) = *color; }
			inline bool IsShowBordersEnabled() { return mShowBorders; }

			void Init(const std::wstring& fileName, PageIndexer* indexer, const VTDesc* info);
			void Dispose();
			void Update(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, uint32_t updateCount);
			void Submit(const Page& request);
			void Clear();
			void Restart();
		private:
			bool LoadPage(ReadState& state);
			void PageLoadComplete(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, const ReadState& state);
			bool CopyBorder(data_ptr& imgData);

			const VTDesc* mInfo = nullptr;
			const PageIndexer* mIndexer = nullptr;

			TileDataFile mFile;

			std::function<void(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>, const Page&, const data_ptr&)> mOnLoadComplete
				= [](Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>, const Page&, const data_ptr&) {};

			PageThread<ReadState> mReadThread;

			bool mShowBorders = false;
			struct
			{
				float r;
				float g;
				float b;
				float a;
			} mBorderColor = { 0.0, 1.0, 0.0, 1.0 };
		};
	}
}

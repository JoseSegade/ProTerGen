#pragma once

#include <functional>
#include "CommonHeaders.h"

#include "VirtualTextureCommon.h"
#include "PageLoaderFromDisk.h"
#include "ImageRawData.h"


namespace ProTerGen
{
	namespace VT
	{
		class TileGenerator
		{
		public:
			TileGenerator() = default;
			virtual ~TileGenerator() = default;

			virtual void OnFinish(const std::function<void(void)>& func) = 0;
			virtual void CloseSignal() = 0;
			virtual void Init(const std::wstring& filename, VTDesc* desc) = 0;
			virtual void Init(const std::string& filename, VTDesc* desc) = 0;
			virtual void Generate() = 0;
			virtual void Close() = 0;
		};

		class BMPTileGenerator : public TileGenerator
		{
		public:
			BMPTileGenerator() = default;
			~BMPTileGenerator();

			inline void OnFinish(const std::function<void(void)>& func) override { mOnFinish = func; }
			void CloseSignal() override;

			void Init(const std::wstring& filename, VTDesc* desc) override;
			void Init(const std::string& filename, VTDesc* desc) override;
			void Generate() override;

			void Close() override;
		private:
			void CopyTile(ImageRawData& image, const Page& request);

		private:
			const char IncompleteChar = 'i';
			const char CompleteChar = 'c';

			VTDesc* mInfo = nullptr;
			PageIndexer mPageIndexer;
			TileDataFile mTileDataFile;
			FileRawData mFile;
			std::atomic_bool mRun;
			std::function<void(void)> mOnFinish = []() {};

			ImageRawData mRawDataPage;
			ImageRawData mRawData2xTile;
		};

		
	}
}
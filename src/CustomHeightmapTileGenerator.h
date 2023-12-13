#pragma once

#include "CommonHeaders.h"
#include "GpuBatches.h"
#include "Shaders.h"
#include "Materials.h"

namespace ProTerGen
{
	namespace VT
	{
		class CustomHeightmapTileGenerator
		{
		public:
			struct HeightmapConfig
			{
				uint32_t Width = 0;
				uint32_t Height = 0;
				uint32_t TileWidth = 0;
				uint32_t TileHeight = 0;
			};
		public:
			CustomHeightmapTileGenerator() {};
			~CustomHeightmapTileGenerator();

			inline void OnFinish(const std::function<void(void)>& func) { mOnFinish = func; }

			bool TryLoadCustomHeightmap(const std::wstring& filename);
			void OpenForWritting(const std::wstring& filename);
			void Init
			(
				Microsoft::WRL::ComPtr<ID3D12Device> device,	
				const HeightmapConfig& config,
				GpuComputePipeline& pipeline,
				Shaders& shaders, 
				Materials& materials,
				DescriptorHeaps& descriptorHeaps				
			);
			void StartThread
			(
				const std::function<void(void)>& executePipeline,
				GpuComputePipeline& pipeline,
				Shaders& shaders,
				Materials& materials,
				DescriptorHeaps& descriptorHeaps
			);
			void Abort();

			void Close();

		private:
			enum PSOs : uint32_t
			{
				DEFAULT = 0,
				COUNT = DEFAULT + 1
			};

			struct Metadata;

			Metadata ReadFileMetadata(FILE*& file) const;
			void WriteFileMetadata(FILE*& file, Metadata metadata) const;
			void WriteIscompleteMetadata(FILE* file, bool isComplete) const;			
			void Run
			(
				GpuComputePipeline& pipeline,
				Shaders& shaders,
				Materials& materials,
				DescriptorHeaps& descriptorHeaps
			);

		private:
			std::function<void(void)> mOnFinish = []() {};
			HeightmapConfig mConfig = {};

			struct HeightFile
			{
				FILE* File = nullptr;
				size_t Offset = 0;

				~HeightFile()
				{
					if (File != nullptr)
					{
						fclose(File);
						File = nullptr;
					}
				}
			} mFile = {};

			std::function<void(void)> mExecutePipeline = []() {};

		public:
			std::jthread mThread;
			std::atomic<bool> mRun;

		};
	}
}

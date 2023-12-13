#pragma once

#include "VirtualTextureCommon.h"

#include <functional>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <memory>

#include "PageThread.h"
#include "GpuBatches.h"
#include "Shaders.h"
#include "Materials.h"
#include "TerrainLayer.h"

namespace ProTerGen
{
	namespace VT
	{
		struct ComputeContext
		{
			Microsoft::WRL::ComPtr<ID3D12Device> device = nullptr;
			GpuComputePipeline& pipeline;
			Shaders& shaders;
			Materials& materials;
			DescriptorHeaps& descriptorHeaps;
			union
			{
				std::function<void(void)> dispatchExecute = []() {};
				std::function<void(GpuComputePipeline&)> dispatchExecuteArgs;
			};
			~ComputeContext() {};
		};

		template<class T>
		class GpuPageGenerator
		{
		public:
			struct GenerationTextures  
			{
				static const uint32_t COUNT = 0;
			};
		public:
			// Something called CRTP (Curiously Recurring Template Pattern) allows this magic.

			static constexpr uint32_t TEXTURES_COUNT()
			{
				return T::_TEXTURES_COUNT();
			}
			static constexpr auto TEXTURES_FORMAT()
			{
				return T::_TEXTURES_FORMAT();
			}
			static constexpr auto TEXTURES_BYTES_PER_TEXEL()
			{
				return T::_TEXTURES_BYTES_PER_TEXEL();
			}
		public:
			using load_complete_f = std::function<void(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>, const MultiPage&)>;
			virtual ~GpuPageGenerator() {};
			virtual void Init
			(
				PageIndexer* indexer,
				const VTDesc* info,
				std::unique_ptr<ComputeContext> computeContext,
				uint32_t tilesPerDispatch
			) = 0;
			virtual void Dispose() = 0;
			virtual void Update(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, uint32_t updateCount) = 0;
			virtual void Reload() = 0;
			virtual void Submit(const Page& request) = 0;
			virtual void Clear() = 0;
			virtual void Restart() = 0;

			virtual void OnLoadComplete(load_complete_f newFunc) = 0;

			virtual bool IsShowBordersEnabled() const = 0;
			virtual void EnableShowBorders(bool value) = 0;
		protected:
			virtual bool LoadPage(MultiPage& readState) = 0;
			virtual void OnProcessingComplete(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, const MultiPage& page) = 0;

			template<typename T, size_t _Size> 
			void ColorBorders(void*& data, const std::array<T, _Size>& color, uint32_t tileSize, uint32_t borderSize, size_t alignedRowPitch) const
			{
				const size_t offsetHBytes = alignedRowPitch * borderSize;
				const size_t offsetVBytes = (size_t)borderSize * _Size;

				const size_t offsetFirstRow = (offsetVBytes + offsetHBytes);
				for (uint32_t i = 0; i < tileSize; ++i)
				{
					for (uint32_t j = 0; j < _Size; ++j)
					{
						const size_t index = offsetFirstRow + (i * _Size) + j;
						((T*)data)[index] = color[j];
					}
				}

				const size_t offsetLastRow = offsetHBytes + (alignedRowPitch * ((size_t)tileSize - 1)) + offsetVBytes;
				for (uint32_t i = 0; i < tileSize; ++i)
				{
					for (uint32_t j = 0; j < _Size; ++j)
					{
						const size_t index = offsetLastRow + (i * _Size) + j;
						((T*)data)[index] = color[j];
					}
				}
				for (uint32_t i = 1; i < tileSize - 1; ++i)
				{
					const size_t offsetRow = offsetHBytes + (alignedRowPitch * i) + offsetVBytes;
					for (uint32_t j = 0; j < _Size; ++j)
					{
						const size_t index = offsetRow + j;
						((T*)data)[index] = color[j];
					}
					for (uint32_t j = 0; j < _Size; ++j)
					{
						const size_t index = offsetRow + (((size_t)tileSize - 1) * _Size) + j;
						((T*)data)[index] = color[j];
					}
				}
			}

		};

		class PageGpuGen_HNC;
		template<>
		struct GpuPageGenerator<PageGpuGen_HNC>::GenerationTextures
		{			
			static const uint32_t HEIGHTMAP = 0;
			static const uint32_t NORMALMAP = HEIGHTMAP + 1;
			static const uint32_t COLOR = NORMALMAP + 1;
			static const uint32_t COUNT = COLOR + 1;
		};

		class PageGpuGen_HNC : public GpuPageGenerator<PageGpuGen_HNC>
		{
		public:
				
			static constexpr uint32_t _TEXTURES_COUNT() { return GenerationTextures::COUNT; }
			static constexpr const std::array<DXGI_FORMAT, GenerationTextures::COUNT> _TEXTURES_FORMAT() 
			{
				return { VT_HEIGHTMAP_FORMAT, VT_NORMAL_FORMAT, VT_COLOR_ALBEDO_FORMAT };
			};
			static constexpr const std::array<uint32_t, GenerationTextures::COUNT> _TEXTURES_BYTES_PER_TEXEL()
			{
				return { VT_HEIGHTMAP_BYTES_PER_PIXEL, VT_NORMAL_BYTES_PER_PIXEL, VT_COLOR_ALBEDO_BYTES_PER_PIXEL };
			}

		public:

			virtual ~PageGpuGen_HNC();

			void Init
			(
				PageIndexer* indexer,
				const VTDesc* info,
				std::unique_ptr<ComputeContext> computeContext,
				uint32_t tilesPerDispatch
			);
			void Dispose();
			void Update(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, uint32_t updateCount);
			void Reload();
			void Submit(const Page& request);
			void Clear();
			void Restart();

			inline void OnLoadComplete(load_complete_f newFunc) { mOnLoadComplete = newFunc; }

			inline bool IsShowBordersEnabled() const { return mShowBordersEnabled; }
			inline void EnableShowBorders(bool value) { mShowBordersEnabled = value; }

		private:
			enum ColorLayer : uint32_t
			{
				BASE = 0,
				HEIGHT = BASE + 1,
				CURVATURE = HEIGHT + 1
			};
			enum class PSOs : uint32_t
			{
				DEFAULT = 0,
				COUNT = DEFAULT + 1
			};
			struct TileColorTextureLayer
			{
				uint32_t TextureSlot = 0;
				uint32_t ColorLayer = 0;
				float MinValue = 0;
				float MaxValue = 0;
			};
			struct ComputeTileRequestConstants
			{
				DirectX::XMFLOAT2 TerrainSize = { 1.0f, 1.0f };
				DirectX::XMFLOAT2 TileSize = { 1.0f, 1.0f };
				DirectX::XMFLOAT2 Min = { 0.0f, 0.0f };
				DirectX::XMFLOAT2 Max = { 0.0f, 0.0f };
				float Scale = 1.0f;
				uint32_t LayerCount = 0;
				TileColorTextureLayer ColorTextureLayers[8] = {};
			};

		protected:
			bool LoadPage(MultiPage& readState);
			void OnProcessingComplete(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, const MultiPage& page);

		private:
			bool mShowBordersEnabled = false;
			uint32_t mTilesPerDispatch = 0;

			const PageIndexer* mIndexer = nullptr;
			const VTDesc* mInfo = nullptr;
			std::unique_ptr<ComputeContext> mComputeContext = nullptr;
			
			PageThread<MultiPage> mPageThread;

			load_complete_f mOnLoadComplete
				= [](Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>, const MultiPage&) {};
		};

		class PageGpuGen_Sdh;
		template<>
		struct GpuPageGenerator<PageGpuGen_Sdh>::GenerationTextures
		{			
			static const uint32_t NORMAL_HEIGHTMAP = 0;
			static const uint32_t COUNT = NORMAL_HEIGHTMAP + 1;
		};

		// Sdh: slope (float), derivatives (float2), height (float)
		class PageGpuGen_Sdh : public GpuPageGenerator<PageGpuGen_Sdh>
		{
		public:
			static constexpr uint32_t _TEXTURES_COUNT() { return GenerationTextures::COUNT; }
			static constexpr const std::array<DXGI_FORMAT, GenerationTextures::COUNT> _TEXTURES_FORMAT() 
			{
				return { DXGI_FORMAT_R32G32B32A32_FLOAT };
			};
			static constexpr const std::array<uint32_t, GenerationTextures::COUNT> _TEXTURES_BYTES_PER_TEXEL()
			{
				return { 16 };
			}

		public:
			virtual ~PageGpuGen_Sdh();

			void Init
			(
				PageIndexer* indexer,
				const VTDesc* info,
				std::unique_ptr<ComputeContext> computeContext,
				uint32_t tilesPerDispatch
			);
			void Dispose();
			void Update(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, uint32_t updateCount);
			void Reload();
			void Submit(const Page& request);
			void Clear();
			void Restart();

			void SetLayers(const std::vector<Layer>& layers);

			inline void OnLoadComplete(load_complete_f newFunc) { mOnLoadComplete = newFunc; }

			inline bool IsShowBordersEnabled() const { return mShowBordersEnabled; }
			inline void EnableShowBorders(bool value) { mShowBordersEnabled = value; }

		private:
			struct ComputeNhCB
			{
				DirectX::XMFLOAT2 TerrainSize = { 1.0f, 1.0f };
				DirectX::XMFLOAT2 TileSize    = { 1.0f, 1.0f };
				DirectX::XMFLOAT2 Min         = { 0.0f, 0.0f };
				DirectX::XMFLOAT2 Max         = { 0.0f, 0.0f };
				uint32_t          Mip         = 0;
				DirectX::XMFLOAT3 __pad {};
			};
			struct ComputeNoiseCB
			{
				float    amplitude   = 1.0f;
				float    frecuency   = 1.0f;
				float    gain        = 1.0f;
				uint32_t octaves     = 1;
				float    seed        = 1.0f;
				float    layerWeight = 1.0f;
				uint32_t index       = 1;
				float    _pad = 0.0f;
			};
			enum class PSOs : uint32_t
			{
				HEIGHT    = 0,
				SLOPE_DER = HEIGHT    + 1,
				COUNT     = SLOPE_DER + 1,
			};
			enum class CBs : uint32_t
			{
				TERRAIN = 0,
				NOISE   = TERRAIN + 1,
				COUNT   = NOISE + 1,
			};

		protected:
			bool LoadPage(MultiPage& readState);
			void OnProcessingComplete(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, const MultiPage& page);

		private:
			bool mShowBordersEnabled = false;
			uint32_t mTilesPerDispatch = 0;

			const PageIndexer* mIndexer = nullptr;
			const VTDesc* mInfo = nullptr;
			std::unique_ptr<ComputeContext> mComputeContext = nullptr;
			size_t mLayerCount = 1;
			
			PageThread<MultiPage> mPageThread;

			load_complete_f mOnLoadComplete
				= [](Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>, const MultiPage&) {};
		};
	}
}

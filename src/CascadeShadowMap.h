#pragma once

#include <array>
#include <unordered_map>
#include <functional>
#include "CommonHeaders.h"
#include <DirectXMath.h>
#include "ShaderConstants.h"
#include "MathHelpers.h"
#include "DescriptorHeaps.h"
#include "CameraSystem.h"
#include "FrameResources.h"

namespace ProTerGen
{
	class CascadeShadowMap
	{
	public:
		using draw_func_t = std::function<void()>;

		DirectX::XMFLOAT3 shadowPos{ 0.0f, 0.0f, 0.0f };

		CascadeShadowMap() {};
		CascadeShadowMap(Light& sun, CameraComponent& cam) : mLight(&sun), mCamera(&cam) {};
		~CascadeShadowMap() {};

		void SetSun(const Light& sun) { mLight = &sun; };
		void SetCamera(const CameraComponent& cam) { mCamera = &cam; };
		void SetFrameAddress(FrameResources** frame) { mFrame = frame; };
		void SetPerShadowBufferShadowRegister(uint32_t reg) { mPerShadowBufferShaderRegister = (int32_t)reg; }
		void Init(Microsoft::WRL::ComPtr<ID3D12Device> device, DescriptorHeaps& heaps, size_t resolution = 1024);
		void Update(double deltaTime);
		void Clear(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, DescriptorHeaps& descriptorHeaps);
		void Draw(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, DescriptorHeaps& descriptorHeaps, draw_func_t drawFunc);

		uint32_t GetShadowSRVHeapLocation() const { return mShadowSRVHeapLocation; }
		constexpr D3D12_VIEWPORT GetViewport() const
		{
			return D3D12_VIEWPORT
			{
				.TopLeftX = 0,
				.TopLeftY = 0,
				.Width    = (float)mShadowResolution,
				.Height   = (float)mShadowResolution,
				.MinDepth = 0.0f,
				.MaxDepth = 1.0f
			};
		};
		constexpr D3D12_RECT GetScissorRect() const
		{
			return D3D12_RECT
			{
				.left   = 0,
				.top    = 0,
				.right  = (int32_t)mShadowResolution,
				.bottom = (int32_t)mShadowResolution
			};
		}

		void StoreShadowConstants(ShadowConstants& shadowConstants) const;
		void FillShadowStructuredBuffer(UploadBuffer<ShadowSplitConstants>* buffer);
	private:
		FrameResources** mFrame         = nullptr;
		Light const* mLight             = nullptr;
		CameraComponent const * mCamera = nullptr;

		int32_t mPerShadowBufferShaderRegister = -1;
		size_t mShadowResolution               = 0;

		struct DepthTextures
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
		};
		std::array<DepthTextures, gNumCascadeShadowMaps> mDepthTextures{};
		uint32_t mShadowSRVHeapLocation = 0;

		std::array<DirectX::XMFLOAT4X4, gNumCascadeShadowMaps> mShadowMats{};
		std::array<float, gNumCascadeShadowMaps> mShadowSplits{};

		const float mBias    = 0.0f;
		const float mMinDist = 0.0f;
		const float mMaxDist = 1.0f;
	};
}

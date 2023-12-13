#include "CascadeShadowMap.h"
#include <math.h>

void ProTerGen::CascadeShadowMap::Init(Microsoft::WRL::ComPtr<ID3D12Device> device, DescriptorHeaps& heaps, size_t resolution)
{
	mShadowResolution = resolution;

	mShadowSRVHeapLocation = heaps.SetContinuousSrvIndices("CASCADE_SHADOW_MAPS", "CSM_map", 4);
	for (uint32_t i = 0; i < gNumCascadeShadowMaps; ++i)
	{
		{
			// Create the resource
			const D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
			const D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D
			(
				DXGI_FORMAT_R32_TYPELESS,
				mShadowResolution,
				(uint16_t)mShadowResolution,
				1,
				1,
				1,
				0,
				D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
			);
			const D3D12_CLEAR_VALUE clearValue = { .Format = DXGI_FORMAT_D32_FLOAT, .DepthStencil = {.Depth = 1.0f, .Stencil = 0 } };
			ThrowIfFailed(device->CreateCommittedResource
			(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&resDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				&clearValue,
				IID_PPV_ARGS(mDepthTextures[i].Resource.ReleaseAndGetAddressOf())
			));
			mDepthTextures[i].Resource->SetName((L"CascadeShadowMap_Depth_" + std::to_wstring(i)).c_str());
		}

		{
			// Create the shader views
			const D3D12_DEPTH_STENCIL_VIEW_DESC dsvd
			{
				.Format = DXGI_FORMAT_D32_FLOAT,
				.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
				.Flags = D3D12_DSV_FLAG_NONE,
				.Texture2D =
				{
					.MipSlice = 0
				},
			};
			const uint32_t heapDepthIdx = heaps.SetDsvIndex("CascadeShadowMap_Depth_" + std::to_string(i));
			const D3D12_CPU_DESCRIPTOR_HANDLE cpuDepthHandle = heaps.GetDsvHeap().GetCPUHandle(heapDepthIdx);
			device->CreateDepthStencilView
			(
				mDepthTextures[i].Resource.Get(),
				&dsvd,
				cpuDepthHandle
			);

			const D3D12_SHADER_RESOURCE_VIEW_DESC srvd
			{
				.Format = DXGI_FORMAT_R32_FLOAT,
				.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
				.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
				.Texture2D
				{
					.MostDetailedMip = 0,
					.MipLevels = 1,
					.PlaneSlice = 0,
					.ResourceMinLODClamp = 0.0f,
				}
			};
			const D3D12_CPU_DESCRIPTOR_HANDLE cpuSrvHandle = heaps.GetSrvHeap().GetCPUHandle(mShadowSRVHeapLocation + i);
			device->CreateShaderResourceView
			(
				mDepthTextures[i].Resource.Get(),
				&srvd,
				cpuSrvHandle
			);
		}

	}
}

#ifdef _DEBUG
void PrintV(const DirectX::XMVECTOR& v, const std::string& name = "")
{
	printf("%s [%.3f, %.3f, %.3f, %.3f]\n", name.c_str(), v.m128_f32[0], v.m128_f32[1], v.m128_f32[2], v.m128_f32[3]);
}

void PrintM(const DirectX::XMMATRIX& m, const std::string name = "")
{
	printf
	(
		"%s { [%.3f, %.3f, %.3f, %.3f]  \n\t  [%.3f, %.3f, %.3f, %.3f]  \n\t  [%.3f, %.3f, %.3f, %.3f]  \n\t  [%.3f, %.3f, %.3f, %.3f] }\n",
		name.c_str(),
		m.r[0].m128_f32[0], m.r[0].m128_f32[1], m.r[0].m128_f32[2], m.r[0].m128_f32[3],
		m.r[1].m128_f32[0], m.r[1].m128_f32[1], m.r[1].m128_f32[2], m.r[1].m128_f32[3],
		m.r[2].m128_f32[0], m.r[2].m128_f32[1], m.r[2].m128_f32[2], m.r[2].m128_f32[3],
		m.r[3].m128_f32[0], m.r[3].m128_f32[1], m.r[3].m128_f32[2], m.r[3].m128_f32[3]
	);
}

void PrintFrustumPoints(const std::array<DirectX::XMVECTOR, 8> points)
{
	printf("Frustum Points\n");
	for (uint32_t i = 0; i < 8; ++i)
	{
		PrintV(points[i], std::to_string(i));
	}
}
#else
#define PrintV(x, y) ((void)0)
#define PrintM(x, y) ((void)0)
#define PrintFrustumPoints(x) ((void)0)
#endif

std::array<float, ProTerGen::gNumCascadeShadowMaps> ComputeCascadeSplits(float minDistance, float maxDistance)
{
	std::array<float, ProTerGen::gNumCascadeShadowMaps> res{};

	const float range = maxDistance - minDistance;
	const float invRange = 1.0f / range;

	for (uint32_t i = 0; i < ProTerGen::gNumCascadeShadowMaps; ++i)
	{
		const float div = 2.0f / (float)(1 << (ProTerGen::gNumCascadeShadowMaps - i));
		res[i] = invRange * div;
	}
	return res;
}

void ComputeWorldPositionFrustumPoints(std::array<DirectX::XMVECTOR, 8>& points, DirectX::XMMATRIX invViewProjMat)
{
	for (uint32_t i = 0; i < 8; ++i)
	{
		points[i] = DirectX::XMVector3TransformCoord(points[i], invViewProjMat);
	}
}

void ScaleFrustumPointsToSplit(std::array<DirectX::XMVECTOR, 8>& points, float minSplitDistance, float maxSplitDistance)
{
	for (uint32_t i = 0; i < 4; ++i)
	{
		DirectX::XMVECTOR frustumV = DirectX::XMVectorSubtract(points[(size_t)i + 4], points[(size_t)i]);
		frustumV.m128_f32[3] = 0.0f;
		const DirectX::XMVECTOR farV     = DirectX::XMVectorScale(frustumV, maxSplitDistance);
		const DirectX::XMVECTOR nearV    = DirectX::XMVectorScale(frustumV, minSplitDistance);

		points[(size_t)i + 4] = DirectX::XMVectorAdd(points[(size_t)i], farV);
		points[(size_t)i]     = DirectX::XMVectorAdd(points[(size_t)i], nearV);
	}
}

DirectX::XMVECTOR GetFrustumCenter(const std::array<DirectX::XMVECTOR, 8>& points)
{
	const DirectX::XMVECTOR nc = DirectX::XMVectorAdd(points[0], DirectX::XMVectorScale(DirectX::XMVectorSubtract(points[2], points[0]), 0.5f));
	const DirectX::XMVECTOR fc = DirectX::XMVectorAdd(points[4], DirectX::XMVectorScale(DirectX::XMVectorSubtract(points[6], points[4]), 0.5f));
	return                       DirectX::XMVectorAdd(       nc, DirectX::XMVectorScale(DirectX::XMVectorSubtract(       fc,        nc), 0.5f));
}

float ComputeMaxRadius(std::array<DirectX::XMVECTOR, 8>& points, const DirectX::XMVECTOR& center)
{
	DirectX::XMVECTOR res = { 0.0f, 0.0f, 0.0f, 0.0f };
	for (uint32_t i = 0; i < 8; ++i)
	{
		const DirectX::XMVECTOR distanceV = DirectX::XMVector3Length(DirectX::XMVectorSubtract(points[(size_t)i], center));
		res = DirectX::XMVectorMax(res, distanceV);
	}
	return DirectX::XMVectorGetX(res);
}

DirectX::XMMATRIX ComputeSplitLightViewMatrix(const DirectX::XMVECTOR& lightDir, const DirectX::XMVECTOR& center, float radius)
{
	const bool isLightDirUp       = DirectX::XMVectorGetX(lightDir) == 0 && DirectX::XMVectorGetZ(lightDir) == 0;
	const DirectX::XMVECTOR up    = isLightDirUp ? DirectX::XMVECTOR{ 1.0f, 0.0f, 0.0f, 0.0f } : DirectX::XMVECTOR{ 0.0f, 1.0f, 0.0f, 0.0f };
	const DirectX::XMVECTOR lPosV = DirectX::XMVectorSubtract(center, DirectX::XMVectorScale(lightDir, radius));
	return DirectX::XMMatrixLookAtLH(lPosV, center, up);
}

DirectX::XMMATRIX ComputeSplitLightOrthoMatrix(float radius)
{
	const float diam = radius * 2.0f;
	return DirectX::XMMatrixOrthographicLH(diam, diam, 0.0f, diam);
}

void ProTerGen::CascadeShadowMap::Update(double deltaTime)
{	
	const std::array<float, gNumCascadeShadowMaps> cascadeSplits = ComputeCascadeSplits(mMinDist, mMaxDist);

	const DirectX::XMMATRIX cProj        = DirectX::XMLoadFloat4x4(&mCamera->Projection);
	const DirectX::XMMATRIX cView        = DirectX::XMLoadFloat4x4(&mCamera->View);
	const DirectX::XMMATRIX cViewProj    = DirectX::XMMatrixMultiply(cView, cProj);
	const DirectX::XMMATRIX cInvViewProj = DirectX::XMMatrixInverse(nullptr, cViewProj);

	for (uint32_t splitNumber = 0; splitNumber < gNumCascadeShadowMaps; ++splitNumber)
	{
		const float prevSplitDistance = splitNumber == 0 ? mMinDist : cascadeSplits[(size_t)splitNumber - 1];
		const float currSplitdistance = cascadeSplits[(size_t)splitNumber];

		std::array<DirectX::XMVECTOR, 8> frustumPoints =
		{
			DirectX::XMVECTOR{ -1.0f,  1.0f, 0.0f, 1.0f },
			DirectX::XMVECTOR{  1.0f,  1.0f, 0.0f, 1.0f },
			DirectX::XMVECTOR{  1.0f, -1.0f, 0.0f, 1.0f },
			DirectX::XMVECTOR{ -1.0f, -1.0f, 0.0f, 1.0f },

			DirectX::XMVECTOR{ -1.0f,  1.0f,  1.0f, 1.0f },
			DirectX::XMVECTOR{  1.0f,  1.0f,  1.0f, 1.0f },
			DirectX::XMVECTOR{  1.0f, -1.0f,  1.0f, 1.0f },
			DirectX::XMVECTOR{ -1.0f, -1.0f,  1.0f, 1.0f },
		};

		ComputeWorldPositionFrustumPoints(frustumPoints, cInvViewProj);
		ScaleFrustumPointsToSplit(frustumPoints, prevSplitDistance, currSplitdistance);

		const DirectX::XMVECTOR centerV = GetFrustumCenter(frustumPoints);
		const float radius              = ComputeMaxRadius(frustumPoints, centerV);

		const DirectX::XMVECTOR sunDirV = { mLight->Direction.x, mLight->Direction.y, -mLight->Direction.z };
		const DirectX::XMVECTOR up = (DirectX::XMVectorGetX(sunDirV) == 0 && DirectX::XMVectorGetZ(sunDirV) == 0) 
			? DirectX::XMVECTOR{ 0.0f, 0.0f, 1.0f } : DirectX::XMVECTOR{ 0.0f, 1.0f, 0.0f};

		const float ra = 50.0f;
		const DirectX::XMVECTOR tar = DirectX::XMLoadFloat3(&shadowPos);
		const DirectX::XMVECTOR eye = DirectX::XMVectorAdd(tar, DirectX::XMVectorScale(sunDirV, -ra));
		const DirectX::XMMATRIX lViewMat = DirectX::XMMatrixLookAtLH(eye, tar, up);

		DirectX::XMFLOAT3 pos{};
		DirectX::XMStoreFloat3(&pos, DirectX::XMVector3Transform(tar, lViewMat));
		const float l = pos.x - ra;
		const float r = pos.x + ra;
		const float b = pos.y - ra;
		const float t = pos.y + ra;
		const float n = pos.z - ra;
		const float f = pos.z + ra;
		const DirectX::XMMATRIX lOrthoMat = DirectX::XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

		const DirectX::XMMATRIX splitMat = DirectX::XMMatrixMultiply(lViewMat, lOrthoMat);
		DirectX::XMStoreFloat4x4(&mShadowMats[(size_t)splitNumber], splitMat);

		const float clipDistance = mCamera->Far - mCamera->Near;
		mShadowSplits[(size_t)splitNumber] = -(currSplitdistance * clipDistance + mCamera->Near);

		PerShadowConstants psConstants{};
		DirectX::XMStoreFloat4x4(&psConstants.SplitShadowMatrix, DirectX::XMMatrixTranspose(splitMat));
		(*mFrame)->PerShadowCB->CopyData(splitNumber, psConstants);
	}
}

void ProTerGen::CascadeShadowMap::Clear(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, DescriptorHeaps& descriptorHeaps)
{
	D3D12_RESOURCE_BARRIER barriers[gNumCascadeShadowMaps]{};
	for (uint32_t i = 0; i < gNumCascadeShadowMaps; ++i)
	{
		barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition
		(
			mDepthTextures[i].Resource.Get(),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_STATE_DEPTH_WRITE
		);
	}
	cmdList->ResourceBarrier(gNumCascadeShadowMaps, barriers);
	for (uint32_t splitNumber = 0; splitNumber < gNumCascadeShadowMaps; ++splitNumber)
	{
		const uint32_t dsvIdx = descriptorHeaps.GetIndex("CascadeShadowMap_Depth_" + std::to_string(splitNumber));
		const D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = descriptorHeaps.GetDsvHeap().GetCPUHandle(dsvIdx);

		cmdList->ClearDepthStencilView(cpuHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	}
}

void ProTerGen::CascadeShadowMap::Draw(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, DescriptorHeaps& descriptorHeaps, draw_func_t drawFunc)
{
	constexpr size_t perShadowCBByteSize = Align(sizeof(PerShadowConstants));
	const D3D12_GPU_VIRTUAL_ADDRESS baseAddress = (*mFrame)->PerShadowCB->Resource()->GetGPUVirtualAddress();

	const D3D12_VIEWPORT vp = GetViewport();
	const D3D12_RECT sr = GetScissorRect();
	cmdList->RSSetViewports(1, &vp);
	cmdList->RSSetScissorRects(1, &sr);
	
	D3D12_RESOURCE_BARRIER barriers[gNumCascadeShadowMaps]{};

	for (uint32_t splitNumber = 0; splitNumber < gNumCascadeShadowMaps; ++splitNumber)
	{
		const uint32_t dsvIdx = descriptorHeaps.GetIndex("CascadeShadowMap_Depth_" + std::to_string(splitNumber));
		const D3D12_CPU_DESCRIPTOR_HANDLE gpuHandle = descriptorHeaps.GetDsvHeap().GetCPUHandle(dsvIdx);
		
		D3D12_GPU_VIRTUAL_ADDRESS perShadowAddress = baseAddress + splitNumber * perShadowCBByteSize;

		cmdList->SetGraphicsRootConstantBufferView((uint32_t)mPerShadowBufferShaderRegister, perShadowAddress);
		
		cmdList->OMSetRenderTargets(0, nullptr, TRUE, &gpuHandle);

		drawFunc();
	}

	for (uint32_t i = 0; i < gNumCascadeShadowMaps; ++i)
	{
		barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition
		(
			mDepthTextures[i].Resource.Get(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			D3D12_RESOURCE_STATE_GENERIC_READ
		);
	}
	cmdList->ResourceBarrier(gNumCascadeShadowMaps, barriers);
}

void ProTerGen::CascadeShadowMap::StoreShadowConstants(ShadowConstants& shadowConstants) const 
{
	shadowConstants.Bias = mBias;
}

void ProTerGen::CascadeShadowMap::FillShadowStructuredBuffer(UploadBuffer<ShadowSplitConstants>* buffer)
{
	for (uint32_t splitNumber = 0; splitNumber < gNumCascadeShadowMaps; ++splitNumber)
	{
		const DirectX::XMMATRIX t
		(
			0.5f,  0.0f, 0.0f, 0.0f,
			0.0f, -0.5f, 0.0f, 0.0f,
			0.0f,  0.0f, 1.0f, 0.0f,
			0.5f,  0.5f, 0.0f, 1.0f
		);
		const DirectX::XMMATRIX sMat         = DirectX::XMLoadFloat4x4(&mShadowMats[splitNumber]);
		const DirectX::XMMATRIX sTransformed = DirectX::XMMatrixMultiply(sMat, t);
		ShadowSplitConstants spc{};
		DirectX::XMStoreFloat4x4(&spc.ViewProjMat, DirectX::XMMatrixTranspose(sTransformed));
		spc.SplitDistance = mShadowSplits[splitNumber];
		spc.TextureIndex  = splitNumber;
		buffer->CopyData(splitNumber, spc);
	}
}

#include "RenderSystem.h"
#include "MathHelpers.h"

ProTerGen::RenderSystem::RenderSystem(FrameResources** frameResource, size_t layerCount, Meshes& meshes)
	: mFrame(frameResource), mLayerCount(layerCount), mMeshes(meshes)
{
	mLayers.resize(mLayerCount);
}

void ProTerGen::RenderSystem::Init()
{
}

void ProTerGen::RenderSystem::Update(double deltaTime)
{
	auto& objectCB = (*mFrame)->ObjectCB;
	for (auto& entitySet : mLayers)
	{
		entitySet.clear();
	}
	for (ECS::Entity entity : mEntities)
	{
		MeshRendererComponent& mrc = mRegister->GetComponent<MeshRendererComponent>(entity);
		mLayers[mrc.Layer].insert(entity);

		if (mrc.NumFramesDirty > 0)
		{
			ObjectConstants oc = {};

			DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&mrc.World);
			DirectX::XMMATRIX texTransform = DirectX::XMLoadFloat4x4(&mrc.TexTransform);

			DirectX::XMStoreFloat4x4(&oc.World, DirectX::XMMatrixTranspose(world));
			DirectX::XMStoreFloat4x4(&oc.TexTransform, DirectX::XMMatrixTranspose(texTransform));

			oc.MaterialIndex            = mrc.Mat->MatCBIndex;
			oc.NumTerrainMaterialLayers = mrc.NumTerrainMaterialLayers;

			objectCB->CopyData(mrc.ObjCBIndex, oc);

			--mrc.NumFramesDirty;
		}
	}
}

void ProTerGen::RenderSystem::Render(const RenderContext& ctx)
{
	constexpr size_t objectCBByteSize = Align(sizeof(ObjectConstants));
	const D3D12_GPU_VIRTUAL_ADDRESS baseAddress = (*mFrame)->ObjectCB->Resource()->GetGPUVirtualAddress();
	for (const ECS::Entity& entity : mLayers[ctx.Layer])
	{
		MeshRendererComponent& mrc = mRegister->GetComponent<MeshRendererComponent>(entity);
		MeshGpuLocations mLoc = mMeshes.GetMeshGpuLocation(mrc.MeshGpuLocation, mrc.SubmeshLocation);

		ctx.CommandList->IASetVertexBuffers(0, 1, &mLoc.Vbv);
		ctx.CommandList->IASetIndexBuffer(&mLoc.Ibv);
		ctx.CommandList->IASetPrimitiveTopology(ctx.Topology);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = baseAddress + mrc.ObjCBIndex * static_cast<size_t>(objectCBByteSize);

		ctx.CommandList->SetGraphicsRootConstantBufferView(0, objCBAddress);

		ctx.CommandList->DrawIndexedInstanced
		(
			(uint32_t)mLoc.MeshRenderValues.IndexCount,
			1,
			(uint32_t)mLoc.MeshRenderValues.StartIndexLocation,
			(int32_t)mLoc.MeshRenderValues.BaseVertexLocation,
			0
		);
	}
}

void ProTerGen::RenderSystem::Instance(const RenderContext& ctx)
{
	constexpr size_t objectCBByteSize = Align(sizeof(ObjectConstants));
	const D3D12_GPU_VIRTUAL_ADDRESS baseAddress = (*mFrame)->ObjectCB->Resource()->GetGPUVirtualAddress();
	for (const ECS::Entity& entity : mLayers[ctx.Layer])
	{
		MeshRendererComponent& mrc = mRegister->GetComponent<MeshRendererComponent>(entity);
		MeshGpuLocations mLoc = mMeshes.GetMeshGpuLocation(mrc.MeshGpuLocation, mrc.SubmeshLocation);

		ctx.CommandList->IASetVertexBuffers(0, 1, &mLoc.Vbv);
		ctx.CommandList->IASetIndexBuffer(&mLoc.Ibv);
		ctx.CommandList->IASetVertexBuffers(1, 1, &mLoc.Instances);
		ctx.CommandList->IASetPrimitiveTopology(ctx.Topology);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = baseAddress + mrc.ObjCBIndex * static_cast<size_t>(objectCBByteSize);

		ctx.CommandList->SetGraphicsRootConstantBufferView(0, objCBAddress);

		SubmeshParameters& smp = mLoc.MeshRenderValues;
		ctx.CommandList->DrawIndexedInstanced
		(
			(uint32_t)smp.IndexCount,
			(uint32_t)smp.NumInstances,
			(uint32_t)smp.StartIndexLocation,
			(uint32_t)smp.BaseVertexLocation,
			(uint32_t)smp.BaseInstanceLocation
		);
	}
}

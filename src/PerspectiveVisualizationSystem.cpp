#include "PerspectiveVisualizationSystem.h"


ProTerGen::PerspectiveVisualizationSystem::PerspectiveVisualizationSystem(PassConstants& constants)
    : mRefConstants(constants)
{
}

void ProTerGen::PerspectiveVisualizationSystem::Init()
{
    if (mCamera == ECS::INVALID)
    {
        mCamera = *mEntities.begin();
    }
}

void ProTerGen::PerspectiveVisualizationSystem::Update(double deltaTime)
{
    for (ECS::Entity entity : mEntities)
    {
        CameraComponent& c = mRegister->GetComponent<CameraComponent>(entity);
        if (mCamera == entity && (c.IsDirty || mCameraChanged))
        {
            UpdateViewAndProjectionFromCamera(c);

            DirectX::XMMATRIX view = DirectX::XMLoadFloat4x4(&c.View);
            DirectX::XMMATRIX proj = DirectX::XMLoadFloat4x4(&c.Projection);

            DirectX::XMMATRIX viewProj = DirectX::XMMatrixMultiply(view, proj);
            DirectX::XMMATRIX projView = DirectX::XMMatrixMultiply(proj, view);
            DirectX::XMVECTOR viewDet = DirectX::XMMatrixDeterminant(view);
            DirectX::XMVECTOR projDet = DirectX::XMMatrixDeterminant(proj);
            DirectX::XMVECTOR viewProjDet = DirectX::XMMatrixDeterminant(viewProj);
            DirectX::XMVECTOR projViewDet = DirectX::XMMatrixDeterminant(projView);
            DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(&viewDet, view);
            DirectX::XMMATRIX invProj = DirectX::XMMatrixInverse(&projDet, proj);
            DirectX::XMMATRIX invViewProj = DirectX::XMMatrixInverse(&viewProjDet, viewProj);
            DirectX::XMMATRIX invProjView = DirectX::XMMatrixInverse(&projViewDet, projView);

            DirectX::XMStoreFloat4x4(&mRefConstants.View, DirectX::XMMatrixTranspose(view));
            DirectX::XMStoreFloat4x4(&mRefConstants.InvView, DirectX::XMMatrixTranspose(invView));
            DirectX::XMStoreFloat4x4(&mRefConstants.Proj, DirectX::XMMatrixTranspose(proj));
            DirectX::XMStoreFloat4x4(&mRefConstants.InvProj, DirectX::XMMatrixTranspose(invProj));
            DirectX::XMStoreFloat4x4(&mRefConstants.ViewProj, DirectX::XMMatrixTranspose(viewProj));
            DirectX::XMStoreFloat4x4(&mRefConstants.InvViewProj, DirectX::XMMatrixTranspose(invViewProj));
            DirectX::XMStoreFloat4x4(&mRefConstants.InvProjView, DirectX::XMMatrixTranspose(invProjView));

            mRefConstants.EyePosW = c.Position;
            mRefConstants.RenderTargetSize = DirectX::XMFLOAT2((float)c.Width, (float)c.Height);
            mRefConstants.InvRenderTargetSize = DirectX::XMFLOAT2(1.0f / c.Width, 1.0f / c.Height);
            mRefConstants.NearZ = c.Near;
            mRefConstants.FarZ = c.Far;
            
            mCameraChanged = false;
        }
        if (c.IsDirty)
        {
            UpdateViewAndProjectionFromCamera(c);
        }
    }
}

void ProTerGen::PerspectiveVisualizationSystem::Resize(uint32_t width, uint32_t height)
{
    for (auto& entity : mEntities)
    {
        CameraComponent& c = mRegister->GetComponent<CameraComponent>(entity);
        c.Width = width;
        c.Height = height;
    }
}

void ProTerGen::PerspectiveVisualizationSystem::SetCurrentCamera(ECS::Entity entity)
{
	mCamera = entity;
	mCameraChanged = true;
}

void ProTerGen::PerspectiveVisualizationSystem::SetConstants(PassConstants& passConstants)
{
	mRefConstants = passConstants;
}

ProTerGen::CameraComponent& ProTerGen::PerspectiveVisualizationSystem::MainCameraSettings()
{
    return mRegister->GetComponent<CameraComponent>(mCamera);
}

void ProTerGen::PerspectiveVisualizationSystem::UpdateViewAndProjectionFromCamera(CameraComponent& c)
{
    DirectX::XMVECTOR R = DirectX::XMLoadFloat3(&c.Right);
    DirectX::XMVECTOR U = DirectX::XMLoadFloat3(&c.Up);
    DirectX::XMVECTOR L = DirectX::XMLoadFloat3(&c.Look);
    DirectX::XMVECTOR P = DirectX::XMLoadFloat3(&c.Position);

    L = DirectX::XMVector3Normalize(L);
    U = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(L, R));

    R = DirectX::XMVector3Cross(U, L);

    float x = -DirectX::XMVectorGetX(DirectX::XMVector3Dot(P, R));
    float y = -DirectX::XMVectorGetX(DirectX::XMVector3Dot(P, U));
    float z = -DirectX::XMVectorGetX(DirectX::XMVector3Dot(P, L));

    c.View(0, 0) = c.Right.x;
    c.View(1, 0) = c.Right.y;
    c.View(2, 0) = c.Right.z;
    c.View(3, 0) = x;

    c.View(0, 1) = c.Up.x;
    c.View(1, 1) = c.Up.y;
    c.View(2, 1) = c.Up.z;
    c.View(3, 1) = y;

    c.View(0, 2) = c.Look.x;
    c.View(1, 2) = c.Look.y;
    c.View(2, 2) = c.Look.z;
    c.View(3, 2) = z;

    c.View(0, 3) = 0.0f;
    c.View(1, 3) = 0.0f;
    c.View(2, 3) = 0.0f;
    c.View(3, 3) = 1.0f;

    DirectX::XMMATRIX viewMat = DirectX::XMLoadFloat4x4(&c.View);
    DirectX::XMMATRIX projMat = DirectX::XMMatrixPerspectiveFovLH
    (
        DirectX::XMConvertToRadians(c.Fov), 
        static_cast<float>(c.Width) / c.Height, 
        c.Near, 
        c.Far
    );
    
    XMStoreFloat4x4(&c.Projection, projMat);

    DirectX::XMVECTOR determinant = DirectX::XMMatrixDeterminant(viewMat);
    viewMat = DirectX::XMMatrixInverse(&determinant, viewMat);
    c.Frustum = DirectX::BoundingFrustum(projMat);
    c.Frustum.Transform(c.Frustum, viewMat);

    c.IsDirty = false;
}

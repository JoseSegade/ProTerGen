#include <Windows.h>
#include "CameraSystem.h"
#include "imgui.h"

void ProTerGen::CameraFPSSystem::Init()
{
}

void ProTerGen::CameraFPSSystem::Update(double dt)
{
	Input();

	FPSComponent& fps = mRegister->GetComponent<FPSComponent>(mCamera);
	CameraComponent& c = mRegister->GetComponent<CameraComponent>(mCamera);

	float speed = static_cast<float>(dt) * (mRun ? fps.RunningSpeed : fps.WalkingSpeed);
	Forward(c, mForwardDir * speed);
	Strafe(c, mRightDir * speed);

	if (mCapture)
	{
		POINT mousePos{};
		::GetCursorPos(&mousePos);

		mDeltaX = DirectX::XMConvertToRadians(fps.RotationSpeed * (mousePos.x - mCapturePoint.x));
		mDeltaY = DirectX::XMConvertToRadians(fps.RotationSpeed * (mousePos.y - mCapturePoint.y));
		
		::SetCursorPos(mCapturePoint.x, mCapturePoint.y);

		Pitch(c, mDeltaY);
		Yaw(c, mDeltaX);
	}
}

void ProTerGen::CameraFPSSystem::SetCamera(ECS::Entity entity)
{
	mCamera = entity;
}

void ProTerGen::CameraFPSSystem::Input()
{
	if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
	{
		mCapture = !mCapture;		
		::ShowCursor(mCapture ? FALSE : TRUE);
		::GetCursorPos(&mCapturePoint);
	}

	mForwardDir = ImGui::IsKeyDown(ImGuiKey_W) ? 1.0f : ImGui::IsKeyDown(ImGuiKey_S) ? -1.0f : 0.0f;
	mRightDir = ImGui::IsKeyDown(ImGuiKey_D) ? 1.0f : ImGui::IsKeyDown(ImGuiKey_A) ? -1.0f : 0.0f;

	mRun = ImGui::IsKeyDown(ImGuiKey_LeftShift);
}

void ProTerGen::CameraFPSSystem::Forward(CameraComponent& c, float dx)
{
	if (dx == 0) return;

	c.IsDirty = true;

	DirectX::XMVECTOR s = DirectX::XMVectorReplicate(dx);
	DirectX::XMVECTOR l = DirectX::XMLoadFloat3(&c.Look);
	DirectX::XMVECTOR p = DirectX::XMLoadFloat3(&c.Position);

	DirectX::XMStoreFloat3(&c.Position, DirectX::XMVectorMultiplyAdd(s, l, p));
}

void ProTerGen::CameraFPSSystem::Strafe(CameraComponent& c, float dx)
{
	if (dx == 0) return;

	c.IsDirty = true;

	DirectX::XMVECTOR s = DirectX::XMVectorReplicate(dx);
	DirectX::XMVECTOR r = DirectX::XMLoadFloat3(&c.Right);
	DirectX::XMVECTOR p = DirectX::XMLoadFloat3(&c.Position);

	DirectX::XMStoreFloat3(&c.Position, DirectX::XMVectorMultiplyAdd(s, r, p));

}

void ProTerGen::CameraFPSSystem::Pitch(CameraComponent& c, float da)
{
	c.IsDirty = true;

	DirectX::XMMATRIX r = DirectX::XMMatrixRotationAxis(DirectX::XMLoadFloat3(&c.Right), da);

	DirectX::XMStoreFloat3(&c.Up, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&c.Up), r));
	DirectX::XMStoreFloat3(&c.Look, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&c.Look), r));
}

void ProTerGen::CameraFPSSystem::Yaw(CameraComponent& c, float da)
{
	DirectX::XMMATRIX r = DirectX::XMMatrixRotationY(da);

	DirectX::XMStoreFloat3(&c.Right, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&c.Right), r));
	DirectX::XMStoreFloat3(&c.Up, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&c.Up), r));
	DirectX::XMStoreFloat3(&c.Look, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&c.Look), r));
}


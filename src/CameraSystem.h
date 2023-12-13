#pragma once

#include <DirectXMath.h>
#include <DirectXCollision.h>
#include "ECS.h"

namespace ProTerGen
{
	struct CameraComponent
	{
		DirectX::XMFLOAT3 Position = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

		DirectX::XMFLOAT3 Up = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
		DirectX::XMFLOAT3 Look = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		DirectX::XMFLOAT3 Right = DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f);

		uint32_t Width = 1;
		uint32_t Height = 1;

		float Near = 0.001f;
		float Far = 1000.0f;
		float Fov = 60.0f;

		DirectX::XMFLOAT4X4 View = {};
		DirectX::XMFLOAT4X4 Projection = {};
		DirectX::BoundingFrustum Frustum = {};

		bool IsDirty = true;
	};

	struct FPSComponent
	{
		float WalkingSpeed = 1.0f;
		float RunningSpeed = 100.0f;
		float RotationSpeed = 0.5f;
	};

	class CameraFPSSystem : public ECS::ECSSystem<>
	{
	public:
		void Init() override;
		void Update(double dt) override;

		void SetCamera(ECS::Entity entity);
	protected:
		void Input();
		void Forward(CameraComponent& c, float dx);
		void Strafe(CameraComponent& c, float dx);
		void Pitch(CameraComponent& c, float da);
		void Yaw(CameraComponent& c, float da);

		float mDeltaX = 0.0f;
		float mDeltaY = 0.0f;

		bool mCapture = false;

		float mForwardDir = 0;
		float mRightDir = 0;

		POINT mCapturePoint = { 0, 0 };

		ECS::Entity mCamera = ECS::INVALID;

		bool mRun = false;
	};
}
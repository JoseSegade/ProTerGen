#pragma once

#include "ECS.h"
#include "ShaderConstants.h"
#include "CameraSystem.h"

namespace ProTerGen
{
	class PerspectiveVisualizationSystem : public ECS::ECSSystem<CameraComponent>
	{
	public:
		PerspectiveVisualizationSystem(PassConstants& constants);

		void Init() override;
		void Update(double deltaTime) override;
		void Resize(uint32_t width, uint32_t height);

		void SetCurrentCamera(ECS::Entity entity);
		void SetConstants(PassConstants& passConstants);

		CameraComponent& MainCameraSettings();
	private:
		void UpdateViewAndProjectionFromCamera(CameraComponent& c);

		ECS::Entity mCamera = ECS::INVALID;
		PassConstants& mRefConstants;

		bool mCameraChanged = false;
	};
}

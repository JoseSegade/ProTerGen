#pragma once

#include	<DirectXMath.h>
#include "CommonHeaders.h"
#include "Config.h"

namespace ProTerGen
{
   struct Material
   {
		std::string         Name          = "";
		std::string         Diffuse       = "";
		std::string         Normal        = "";
		DirectX::XMFLOAT4   DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
		DirectX::XMFLOAT4X4 MatTransform  =
		{
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		};
		int32_t  MatCBIndex     = -1;
		uint32_t NumFramesDirty = gNumFrames;
		float    Fresnel        = 0.01f;
		float    Roughness      = 0.5f;		

		constexpr bool operator ==(const Material& rhs) const
		{
			return
				   Name            == rhs.Name
				&& Diffuse         == rhs.Diffuse
				&& Normal          == rhs.Normal
				&& DiffuseAlbedo.x == rhs.DiffuseAlbedo.x
				&& DiffuseAlbedo.y == rhs.DiffuseAlbedo.y
				&& DiffuseAlbedo.z == rhs.DiffuseAlbedo.z
				&& Fresnel         == rhs.Fresnel
				&& Roughness       == rhs.Roughness;
		}
   };
}
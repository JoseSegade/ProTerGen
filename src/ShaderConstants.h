#pragma once

#include <DirectXMath.h>
#include "CommonHeaders.h"
#include "Config.h"

namespace ProTerGen
{
   class MathHelper
   {
   public:
      static constexpr DirectX::XMFLOAT4X4 Identity4x4()
      {
          return DirectX::XMFLOAT4X4
          (
              1.0f, 0.0f, 0.0f, 0.0f,
              0.0f, 1.0f, 0.0f, 0.0f,
              0.0f, 0.0f, 1.0f, 0.0f,
              0.0f, 0.0f, 0.0f, 1.0f
          );
      }
   };

   struct ObjectConstants
   {
      DirectX::XMFLOAT4X4 World                    = MathHelper::Identity4x4();
      DirectX::XMFLOAT4X4 TexTransform             = MathHelper::Identity4x4();
      uint32_t            MaterialIndex            = 0;
      uint32_t            NumTerrainMaterialLayers = 0;
      DirectX::XMUINT2    _Pad0                    = { 0, 0 };
   };

   struct Light
   {
       DirectX::XMFLOAT3 Strength  = { 0.0f, 0.0f, 0.0f };
       float             _Pad0     = 0.0f;
       DirectX::XMFLOAT3 Direction = { 0.0f, 0.0f, 0.0f };
       float             _Pad1     = 0.0f;
   };

   struct Environment
   {
       DirectX::XMFLOAT4 AmbientLight   = { 0.0f, 0.0f, 0.0f, 0.0f };
       DirectX::XMFLOAT4 SkyColor       = { 0.0f, 0.0f, 0.0f, 0.0f };
       DirectX::XMFLOAT4 VanishingColor = { 0.0f, 0.0f, 0.0f, 0.0f };
       DirectX::XMFLOAT4 HorizonColor   = { 0.0f, 0.0f, 0.0f, 0.0f };
       DirectX::XMFLOAT4 CloudColor     = { 0.0f, 0.0f, 0.0f, 0.0f };
       DirectX::XMFLOAT4 FogColor       = { 0.0f, 0.0f, 0.0f, 0.0f };
       float             FogOpacity     = 0.0f;
       DirectX::XMFLOAT2 CloudOffset    = { 0.0f, 0.0f };
       float             CloudScale     = 0.0f;
   };

   struct PassConstants
   {
      DirectX::XMFLOAT4X4 View                = MathHelper::Identity4x4();
      DirectX::XMFLOAT4X4 InvView             = MathHelper::Identity4x4();
      DirectX::XMFLOAT4X4 Proj                = MathHelper::Identity4x4();
      DirectX::XMFLOAT4X4 InvProj             = MathHelper::Identity4x4();
      DirectX::XMFLOAT4X4 ViewProj            = MathHelper::Identity4x4();
      DirectX::XMFLOAT4X4 InvViewProj         = MathHelper::Identity4x4();
      DirectX::XMFLOAT4X4 InvProjView         = MathHelper::Identity4x4();
      DirectX::XMFLOAT2   RenderTargetSize    = { 0.0f, 0.0f };
      DirectX::XMFLOAT2   InvRenderTargetSize = { 0.0f, 0.0f };
      DirectX::XMFLOAT3   EyePosW             = { 0.0f, 0.0f, 0.0f };
      float               NearZ               = 0.0f;
      float               FarZ                = 0.0f;
      float               TotalTime           = 0.0f;
      float               DeltaTime           = 0.0f;
      float               _Pad0               = 0.0f;

      Environment         Env;
      Light               Sun;
   };

   struct MaterialConstants
   {
      DirectX::XMFLOAT4   DiffuseAlbedo   = { 1.0f, 1.0f, 1.0f, 1.0f };
      DirectX::XMFLOAT4X4 MatTransform    = MathHelper::Identity4x4();
      float               Fresnel         = 0.01f;
      float               Roughness       = 0.5f;
      uint32_t            DiffuseMapIndex = 0;
      uint32_t            NormalMapIndex  = 0;
   };

   struct VirtualTextureConstants
   {
       float   VTSize         = 0.0f;
       float   PageTableSize  = 0.0f;
       float   AtlasScale     = 0.0f;
       float   BorderScale    = 0.0f;
       float   BorderOffset   = 0.0f;
       float   MipBias        = 0.0f;
       int32_t AtlasIndex     = -1;
       int32_t PageTableIndex = -1;
   };

   struct TerrainConstants
   {
       float TerrainSize      = 0.0f;
       float TerrianChunkSize = 0.0f;
       float Height           = 0.0f;
       float Subdivision      = 0.0f;
   };

   struct ShadowConstants
   {
       float             Bias  = 0.0f;
       DirectX::XMFLOAT3 _Pad0 = { 1.0f, 0.0f, 0.0f };
   };

   struct ShadowSplitConstants
   {
		DirectX::XMFLOAT4X4 ViewProjMat   = MathHelper::Identity4x4();
        float               SplitDistance = 0.0f;
		uint32_t            TextureIndex  = 0;
   };

   struct PerShadowConstants
   {
       DirectX::XMFLOAT4X4 SplitShadowMatrix;
   };

   struct TerrainMaterialLayerConstants
   {
       int32_t MaterialIndex    = 0;
       float   MinSlope         = 0.0f;
       float   SlopeSmoothness  = 0.0f;
       float   MinHeight        = 0.0f;
       float   HeightSmoothness = 0.0f;
   };
}
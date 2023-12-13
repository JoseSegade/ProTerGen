#pragma once

#include <cstdint>

namespace ProTerGen
{
   constexpr static uint32_t gNumFrames              = 3;
   constexpr static uint32_t gNumFrameRTs            = gNumFrames;
   constexpr static uint32_t gNumFrameDSs            = 1;
   constexpr static uint32_t gFeedbackBufferRTs      = 1;
   constexpr static uint32_t gFeedbackBufferDSs      = 1;

   constexpr static uint32_t gInvalidIndex           = 0xffffffff;


   constexpr static uint32_t gNumIndirectionTextures = 1;
   constexpr static uint32_t gNumAtlasTextures       = 1;
   constexpr static uint32_t gNumAdditionalTextures  = 2;
   constexpr static uint32_t gNumTextures            = 16 + gNumAdditionalTextures;
   constexpr static uint32_t gNumTerrainTextures     = gNumTextures;
   constexpr static uint32_t gNumTexturesImGui       = 1;
   constexpr static uint32_t gNumCascadeShadowMaps   = 4;
   constexpr static uint32_t gNumComputeTextures     = 4;
   constexpr static uint32_t gNumStructuredBuffers   = 2;

   constexpr static uint32_t gNumSRV = gNumIndirectionTextures
	   + gNumAtlasTextures
	   + gNumTextures
	   + gNumTerrainTextures
	   + gNumTexturesImGui
	   + gNumCascadeShadowMaps
	   + gNumComputeTextures;

   constexpr static uint32_t gMaxComputeLayers         = 8;
   constexpr static uint32_t gMaxTerrainMaterialLayers = 8;
   constexpr static uint32_t gWinMinWidth              = 200;
   constexpr static uint32_t gWinMinHeight             = 200;
   const static std::wstring gShadersPath  = L".\\Shaders\\";
   const static std::wstring gTexturesPath = L"..\\res\\Textures\\";
   const static std::wstring gObjsPath     = L"..\\res\\Objs\\";
   const static std::string  gFontsPath    = "..\\res\\Fonts\\";
}

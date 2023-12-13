#pragma once

#include <cinttypes>
#include <functional>
#include <unordered_map>
#include "imgui.h"
#include "ECS.h"

namespace ProTerGen
{
   struct MainConfig
   {
      bool VSync           = false;
      bool Metrics         = false;
      bool Wireframe       = false;
      bool DebugVT         = false;
      bool UpdateVT        = true;
      bool UpdateTerrain   = true;
      bool DebugTerrainLod = false;
      bool ClearVTCache    = false;
      bool DrawBorderColor = false;
      bool ReloadTerrain   = false;
      bool ResizeMaterials = false;
      std::function<void()> OnExit = {};
   };

   enum class EntryType : uint32_t
   {
       BOOL,
       BUTTON,
       UINT32,
       FLOAT,
       FLOAT2,
       FLOAT3,
       FLOAT4,
       COLOR4,
       COLOR3,
       CUSTOM,
   };

   struct Entry
   {
       EntryType   type     = EntryType::BOOL;
       std::string name     = "";
       void*       data     = nullptr;
       bool        disabled = false;
       bool        slider   = false;
       float       min      = 0;
       float       max      = 0;
   };

   struct EntryFunctor
   {
       using entry_func_t = std::function<void()>;
       EntryFunctor(entry_func_t func) : mFunc{ func } {};
       entry_func_t mFunc;
   };

   class MainMenuSystem: public ECS::ECSSystem<MainConfig>
   {
   public:
      void Init() override;
      void Update(double dt) override;

      void RegisterCollection(const std::string& name, std::vector<Entry>& entries, bool open = false);
   private:
      void ShowMetrics();
      void BuildCollections();

      std::unordered_map<std::string, std::pair<std::vector<Entry>, bool>> mCollections;
      bool mShowCollections = false;
   };
   
}

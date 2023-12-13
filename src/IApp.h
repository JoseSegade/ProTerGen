#pragma once

#include "CommonHeaders.h"
#include <functional>

namespace ProTerGen
{
   class IApp
   {
   public:
       using open_file_explorer_func_t = std::function<std::wstring(void)>;

   public:
      IApp() = default;
      virtual ~IApp() = default;

      IApp(IApp&) = delete;
      IApp& operator=(IApp&) = delete;

      virtual bool Init(HWND winHandle, uint32_t width, uint32_t height) = 0;
      virtual void Tick() = 0;
      virtual LRESULT MessageProcessing(HWND winHandle, uint32_t message, WPARAM wParam, LPARAM lParam) = 0;
      virtual void SetOpenFileExplorerFunc(open_file_explorer_func_t f) = 0;
   };
}

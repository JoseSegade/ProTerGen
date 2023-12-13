#pragma once

#include "CommonHeaders.h"
#include "IApp.h"

namespace ProTerGen
{
   class Window
   {
   public:
      static int32_t Run(IApp& app, const std::wstring& name, uint32_t width, uint32_t height);

   protected:
      static HWND Create(IApp& app, const std::wstring& name, uint32_t width, uint32_t height, HINSTANCE& hInst);
      static LRESULT MessageProcessing(HWND hWnd, uint32_t message, WPARAM wParam, LPARAM lParam);
      static int32_t MainLoop(IApp& app);
      static std::wstring OpenFileExplorer();

      static bool mInit;
   };
}
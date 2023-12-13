#pragma once

#include <functional>
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
#include "IApp.h"
#include "Clock.h"
#include "DescriptorHeaps.h"
#include "Config.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
namespace ProTerGen
{
   class App : public IApp
   {
   public:
      App();
      virtual ~App();

      virtual bool        Init(HWND winHandle, uint32_t width, uint32_t height) override;
      virtual LRESULT     MessageProcessing(HWND hwnd, uint32_t msg, WPARAM wParam, LPARAM lParam) override;
      virtual void        Tick() override;
      virtual inline void SetOpenFileExplorerFunc(open_file_explorer_func_t f) override { mOpenFileExplorer = f; }
   
   protected:
      enum StateMask : uint32_t
      {
         Pause      = 1,
         Minimized  = 2,
         Maximized  = 4,
         Resizing   = 8,
         Fullscreen = 16
      };

      float AspectRatio() const;

      virtual void CreateDescriptorHeaps();
      virtual void OnResize();
      virtual void Update(const Clock& gt) = 0;
      virtual void Draw(const Clock& gt) = 0;

      virtual void OnMouseDown(HWND hwnd, WPARAM btnState, int x, int y) { }
      virtual void OnMouseUp(WPARAM btnState, int x, int y) { }
      virtual void OnMouseMove(WPARAM btnState, int x, int y) { }

      bool InitDirect3D(HWND hwnd);
      void CreateCommandObjects();
      void CreateSwapChain(HWND hwnd, Microsoft::WRL::ComPtr<IDXGIFactory4> factory);

      void AddStates(uint32_t states);
      void RemoveStates(uint32_t states);
      bool HasState(StateMask state) const;

      void FlushCommandQueue();

      Microsoft::WRL::ComPtr<ID3D12Resource> GetCurrentRenderBuffer() const;
      D3D12_CPU_DESCRIPTOR_HANDLE            GetCurrentRenderBufferView() const;
      D3D12_CPU_DESCRIPTOR_HANDLE            GetDepthStencilView() const;

      DescriptorHeaps                                   mDescriptorHeaps;
      uint32_t                                          mState              = 0;
      Clock                                             mTimer;
      Microsoft::WRL::ComPtr<IDXGISwapChain>            mSwapChain;
      Microsoft::WRL::ComPtr<ID3D12Device>              mDevice;
      Microsoft::WRL::ComPtr<ID3D12Fence>               mFence;
      uint32_t                                          mCurrentFence;
      Microsoft::WRL::ComPtr<ID3D12CommandQueue>        mCommandQueue;
      Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    mCommandAllocator;
      Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;
      uint32_t                                          mCurrBackBuffer     = 0;
      Microsoft::WRL::ComPtr<ID3D12Resource>            mRenderTargetBuffers[gNumFrames];
      Microsoft::WRL::ComPtr<ID3D12Resource>            mDepthStencilBuffer;
      D3D12_VIEWPORT                                    mScreenViewport;
      D3D12_RECT                                        mScissorRect;
      uint32_t                                          mWidth              = 1;
      uint32_t                                          mHeight             = 1;      
      
      const std::wstring                                mMainWndCaption     = L"ProTerGen";
      const D3D_DRIVER_TYPE                             md3dDriverType      = D3D_DRIVER_TYPE_HARDWARE;
      const DXGI_FORMAT                                 mBackBufferFormat   = DXGI_FORMAT_R8G8B8A8_UNORM;
      const DXGI_FORMAT                                 mDepthStencilFormat = DXGI_FORMAT_D32_FLOAT;

      open_file_explorer_func_t mOpenFileExplorer = []() { return L""; };
   };
}

#include <cassert> 
#include "App.h"
#include <windowsx.h>
#include "resource.h"
#if _DEBUG && PRINT_PERFORMANCE_TIMES
#include "Timer.h"
#endif

ProTerGen::App::App()
	: mCurrentFence(0), mScreenViewport(), mScissorRect()
{
}

ProTerGen::App::~App()
{
	if (mDevice != nullptr)
	{
		FlushCommandQueue();
	}
}

float ProTerGen::App::AspectRatio()const
{
	return static_cast<float>(mWidth) / mHeight;
}

bool ProTerGen::App::Init(HWND winHandle, uint32_t width, uint32_t height)
{
	mWidth = width;
	mHeight = height;
	if (!InitDirect3D(winHandle))
	{
		return false;
	}
	App::OnResize();

	return true;
}

void ProTerGen::App::CreateDescriptorHeaps()
{
	mDescriptorHeaps = ProTerGen::DescriptorHeaps(mDevice, gNumFrames, 1, 0, 0);
}

void ProTerGen::App::OnResize()
{
	assert(mDevice);
	assert(mSwapChain);
	assert(mCommandAllocator);

	FlushCommandQueue();

	ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

	for (uint32_t i = 0; i < gNumFrames; ++i)
	{
		mRenderTargetBuffers[i].Reset();
	}
	mDepthStencilBuffer.Reset();

	ThrowIfFailed(mSwapChain->ResizeBuffers
	(
		gNumFrames,
		mWidth,
		mHeight,
		mBackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING
	));

	mCurrBackBuffer = 0;

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle = mDescriptorHeaps.GetRtvHeap().CpuHandle;
	size_t increment = mDescriptorHeaps.GetRtvHeap().IncrementSize;
	for (UINT i = 0; i < gNumFrames; i++)
	{
		ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mRenderTargetBuffers[i])));
		mDevice->CreateRenderTargetView(mRenderTargetBuffers[i].Get(), nullptr, rtvHeapHandle);
		const std::string name = "APP_RTV_" + std::to_string(i);
		if(mDescriptorHeaps.GetIndex(name) == DescriptorHeaps::InvalidIndex) mDescriptorHeaps.SetRtvIndex(name);
		rtvHeapHandle.Offset(1, (uint32_t)increment);

		mRenderTargetBuffers[i]->SetName((L"APP_SwapBuffer_RT_" + std::to_wstring(i)).c_str());
	}

	D3D12_RESOURCE_DESC depthStencilDesc = {
		.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		.Alignment = 0,
		.Width = mWidth,
		.Height = mHeight,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_R32_TYPELESS,
		.SampleDesc =
		{
			.Count = 1,
			.Quality = 0,
		},
		.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
	};

	D3D12_CLEAR_VALUE optClear =
	{
		.Format = mDepthStencilFormat,
		.DepthStencil = {
			.Depth = 1.0f,
			.Stencil = 0
		}
	};
	CD3DX12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	ThrowIfFailed(mDevice->CreateCommittedResource
	(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())
	));

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc =
	{
		.Format = mDepthStencilFormat,
		.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
		.Flags = D3D12_DSV_FLAG_NONE,
		.Texture2D =
		{
			.MipSlice = 0,

		},
	};

	const std::string name = "APP_DSV";
	if (mDescriptorHeaps.GetIndex(name) == DescriptorHeaps::InvalidIndex) mDescriptorHeaps.SetDsvIndex(name);
	mDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &dsvDesc, GetDepthStencilView());

	mDepthStencilBuffer->SetName(L"APP_SwapBuffer_DS");

	CD3DX12_RESOURCE_BARRIER transitionBarrier = CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	mCommandList->ResourceBarrier(1, &transitionBarrier);

	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	FlushCommandQueue();

	mScreenViewport.TopLeftX = 0;
	mScreenViewport.TopLeftY = 0;
	mScreenViewport.Width = static_cast<float>(mWidth);
	mScreenViewport.Height = static_cast<float>(mHeight);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;

	mScissorRect = { 0, 0, static_cast<LONG>(mWidth), static_cast<LONG>(mHeight) };
}

LRESULT ProTerGen::App::MessageProcessing(HWND hwnd, uint32_t msg, WPARAM wParam, LPARAM lParam)
{
	ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);

	switch (msg)
	{
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			AddStates(StateMask::Pause);
			mTimer.Stop();
		}
		else
		{
			RemoveStates(StateMask::Pause);
			mTimer.Start();
		}
		return 0;
	case WM_SIZE:
		mWidth = LOWORD(lParam);
		mHeight = HIWORD(lParam);
		if (mDevice)
		{
			if (wParam == SIZE_MINIMIZED)
			{
				AddStates(StateMask::Pause | StateMask::Minimized);
				RemoveStates(StateMask::Maximized);
			}
			else if (wParam == SIZE_MAXIMIZED)
			{
				AddStates(StateMask::Maximized);
				RemoveStates(StateMask::Pause | StateMask::Minimized);
				OnResize();
			}
			else if (wParam == SIZE_RESTORED)
			{
				if (HasState(StateMask::Minimized))
				{
					RemoveStates(StateMask::Pause | StateMask::Minimized);
					OnResize();
				}
				else if (HasState(StateMask::Maximized))
				{
					RemoveStates(StateMask::Pause | StateMask::Maximized);
					OnResize();
				}
				else if (HasState(StateMask::Resizing))
				{
				}
				else
				{
					OnResize();
				}
			}
		}
		return 0;
	case WM_ENTERSIZEMOVE:
		AddStates(StateMask::Resizing | StateMask::Pause);
		mTimer.Stop();
		return 0;
	case WM_EXITSIZEMOVE:
		RemoveStates(StateMask::Pause | StateMask::Resizing);
		mTimer.Start();
		OnResize();
		return 0;
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = gWinMinWidth;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = gWinMinHeight;
		return 0;

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		OnMouseDown(hwnd, wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_KEYUP:
		if (wParam == VK_ESCAPE)
		{
			PostQuitMessage(0);
		}
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_MENUCHAR:
		return MAKELRESULT(0, MNC_CLOSE);
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void ProTerGen::App::Tick()
{
	mTimer.Tick();
	if (!HasState(StateMask::Pause))
	{
		Update(mTimer);
		Draw(mTimer);
	}
}

bool ProTerGen::App::InitDirect3D(HWND hWnd)
{
#if defined(DEBUG) || defined(_DEBUG) 
	{
		Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
		debugController->EnableDebugLayer();

		/*
		Microsoft::WRL::ComPtr<ID3D12Debug1> debugController1;
		ThrowIfFailed(debugController->QueryInterface(IID_PPV_ARGS(debugController1.ReleaseAndGetAddressOf())));
		debugController1->SetEnableGPUBasedValidation(true);
		*/
	}
#endif

	Microsoft::WRL::ComPtr<IDXGIFactory4> factory = nullptr;
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(factory.ReleaseAndGetAddressOf())));

	Microsoft::WRL::ComPtr<IDXGIAdapter1> dxgiAdapter1 = nullptr;
	Microsoft::WRL::ComPtr<IDXGIAdapter4> dxgiAdapter4 = nullptr;
	size_t maxDedicatedVideoMemory = 0;
	for (uint32_t i = 0; factory->EnumAdapters1(i, dxgiAdapter1.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
		dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

		if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
			SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(),
				D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
			dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
		{
			maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
			ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
		}
		//OutputDebugStringW(dxgiAdapterDesc1.Description);
		//if(i == 0) ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
	}

	HRESULT hardwareResult = D3D12CreateDevice(
		dxgiAdapter4.Get(),
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(mDevice.ReleaseAndGetAddressOf()));

	if (FAILED(hardwareResult))
	{
		Microsoft::WRL::ComPtr<IDXGIAdapter> pWarpAdapter;
		ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));

		ThrowIfFailed(D3D12CreateDevice
		(
			pWarpAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(mDevice.ReleaseAndGetAddressOf())
		));
	}

	ThrowIfFailed(mDevice->CreateFence
	(
		0,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(mFence.ReleaseAndGetAddressOf())
	));
	mFence->SetName(L"APP_DefaultFence");

	CreateCommandObjects();
	CreateSwapChain(hWnd, factory);
	CreateDescriptorHeaps();

	return true;
}

void ProTerGen::App::CreateCommandObjects()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc =
	{
		.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
		.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE
	};

	ThrowIfFailed(mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));
	mCommandQueue->SetName(L"APP_DefaultCommandQueue");

	ThrowIfFailed(mDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(mCommandAllocator.ReleaseAndGetAddressOf())));
	mCommandAllocator->SetName(L"APP_DefaultCommandAllocator");

	ThrowIfFailed(mDevice->CreateCommandList
	(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		mCommandAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(mCommandList.ReleaseAndGetAddressOf())
	));
	mCommandList->SetName(L"APP_DefaultCommandList");
	mCommandList->Close();
}

void ProTerGen::App::CreateSwapChain(HWND hWnd, Microsoft::WRL::ComPtr<IDXGIFactory4> factory)
{
	mSwapChain.Reset();

	DXGI_SWAP_CHAIN_DESC sd =
	{
		.BufferDesc =
		{
			.Width = mWidth,
			.Height = mHeight,
			.RefreshRate =
			{
				.Numerator = 60,
				.Denominator = 1
			},
			.Format = mBackBufferFormat,
			.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
			.Scaling = DXGI_MODE_SCALING_UNSPECIFIED,
		},
		.SampleDesc = {
			.Count = 1,
			.Quality = 0,
		},
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount = gNumFrames,
		.OutputWindow = hWnd,
		.Windowed = true,
		.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
		.Flags = static_cast<uint32_t>(DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING)
	};

	ThrowIfFailed(factory->CreateSwapChain
	(
		mCommandQueue.Get(),
		&sd,
		mSwapChain.GetAddressOf()
	));
}

void ProTerGen::App::AddStates(uint32_t state)
{
	mState |= state;
}

void ProTerGen::App::RemoveStates(uint32_t state)
{
	mState &= (~state);
}

bool ProTerGen::App::HasState(StateMask state) const
{
	return (mState & state) == state;
}

void ProTerGen::App::FlushCommandQueue()
{
	mCurrentFence++;

	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

	if (mFence->GetCompletedValue() < mCurrentFence)
	{
		HANDLE eventHandle = CreateEvent(NULL, FALSE, FALSE, NULL);

		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));
		assert(eventHandle != 0);

		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

Microsoft::WRL::ComPtr<ID3D12Resource> ProTerGen::App::GetCurrentRenderBuffer() const
{
	return mRenderTargetBuffers[mCurrBackBuffer];
}

D3D12_CPU_DESCRIPTOR_HANDLE ProTerGen::App::GetCurrentRenderBufferView() const
{
	const std::string name = "APP_RTV_" + std::to_string(mCurrBackBuffer);
	return mDescriptorHeaps.GetRtvHeap().GetCPUHandle(mDescriptorHeaps.GetIndex(name));
}

D3D12_CPU_DESCRIPTOR_HANDLE ProTerGen::App::GetDepthStencilView() const
{
	const std::string name = "APP_DSV";
	return mDescriptorHeaps.GetDsvHeap().GetCPUHandle(mDescriptorHeaps.GetIndex(name));
}
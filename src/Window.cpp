#include "Window.h"
#include <commdlg.h>
#include "resource.h"

#define PRINT_PERFORMANCE_TIMES 0
#if _DEBUG && PRINT_PERFORMANCE_TIMES 
#include "Timer.h"
#endif



bool ProTerGen::Window::mInit = false;

int32_t ProTerGen::Window::Run(IApp& app, const std::wstring& name, uint32_t width, uint32_t height)
{
	HINSTANCE hinst = GetModuleHandle(NULL);
	HWND hwnd = Create(app, name, width, height, hinst);

	if (!hwnd)
	{
		return 1;
	}
	mInit = app.Init(hwnd, width, height);
	if (!mInit)
	{
		return 1;
	}

	app.SetOpenFileExplorerFunc(Window::OpenFileExplorer);
	
	int32_t result = MainLoop(app);
	UnregisterClass(name.c_str(), hinst);

	return 0;
}

HWND ProTerGen::Window::Create(IApp& app, const std::wstring& name, uint32_t width, uint32_t height, HINSTANCE& hInst)
{
	HWND hwnd = nullptr;
	WNDCLASSEX wc =
	{
		.cbSize        = sizeof(WNDCLASSEX),
		.style         = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc   = ProTerGen::Window::MessageProcessing,
		.cbClsExtra    = 0,
		.cbWndExtra    = 0,
		.hInstance     = hInst,
		.hIcon         = static_cast<HICON>(LoadImage(hInst, MAKEINTRESOURCE(IDI_ICON),IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR)),
		.hCursor       = LoadCursor(0, IDC_ARROW),
		.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH),
		.lpszMenuName  = 0,
		.lpszClassName = name.c_str()
	};

	if (!RegisterClassEx(&wc))
	{
		MessageBox(0, L"RegisterClass Failed.", 0, 0);
		return hwnd;
	}

	RECT r = { 0, 0, (LONG)width, (LONG)height };
	AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, false);
	
	hInst = wc.hInstance;
	hwnd = CreateWindow
	(
		name.c_str(),
		name.c_str(),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		r.right - r.left,
		r.bottom - r.top,
		0,
		0,
		hInst,
		nullptr
	);
	if (!hwnd)
	{
		MessageBox(0, L"CreateWindow Failed.", 0, 0);
		return hwnd;
	}
	SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&app));

	ShowWindow(hwnd, SW_SHOWDEFAULT);
	UpdateWindow(hwnd);

	return hwnd;
}

LRESULT ProTerGen::Window::MessageProcessing(HWND hWnd, uint32_t message, WPARAM wParam, LPARAM lParam)
{
	ProTerGen::IApp* app = reinterpret_cast<ProTerGen::IApp*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	if (app && mInit)
	{
		return app->MessageProcessing(hWnd, message, wParam, lParam);
	}
	
	return DefWindowProc(hWnd, message, wParam, lParam);
}

int32_t ProTerGen::Window::MainLoop(IApp& app)
{
	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			app.Tick();
		}
	}

	return static_cast<int32_t>(msg.wParam);
}

std::wstring ProTerGen::Window::OpenFileExplorer()
{
	wchar_t file[260] = { 0 };
	OPENFILENAME ofn{};

	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize    = sizeof(ofn);
	ofn.lpstrFile      = file;
	ofn.nMaxFile       = sizeof(file);
	ofn.lpstrFilter    = L"DDS File (*.dds)\0*.dds\0"
		                 L"PNG and JPG Files (*.png, *jpg)\0*.png;*.jpg\0";
	ofn.nFilterIndex   = 1;
	ofn.lpstrFileTitle = nullptr;
	ofn.nMaxFileTitle  = 0;
	ofn.Flags          = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	if (GetOpenFileName(&ofn) == TRUE)
	{
		return std::wstring(file);
	}
	else
	{
		return L"";
	}
}

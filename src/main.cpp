#include "MainEngine.h"
#include "Window.h"
#include "CommonHeaders.h"

#if defined(_DEBUG)
#include <dxgidebug.h>

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

int main(int, char**)
{
	ProTerGen::MainEngine engine = ProTerGen::MainEngine();
	try
	{
		ProTerGen::Window::Run(engine, L"ProTerGen", 1200, 900);
		return 0;
	}
	catch (HrException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}

#if defined(_DEBUG) 
	{
		// Remember to erase all TODO comments before publishing the code.

		Microsoft::WRL::ComPtr<IDXGIDebug1> dxgiDebug;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
		{
			dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
		}

		_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
		_CrtDumpMemoryLeaks();
	}
#endif

	return 0;
}
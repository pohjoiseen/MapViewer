// Program.cpp: MapViewer entry point

#include "framework.h"
#include "HttpClient.h"
#include "TileManager.h"
#include "MapWindow.h"
#include "Resource.h"

// set up libraries to link with using pragmas
#pragma comment(lib, "ComCtl32.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "WinInet.lib")
#pragma comment(lib, "D2d1.lib")
#pragma comment(lib, "Windowscodecs.lib")
// turn on visual styles in a manifest (DPI awareness is turned on in project settings
// but apparently still no setting for this?)
#pragma comment(linker, "\"/manifestdependency:type='win32' \
    name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
    processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // initialize various stuff
    // frame window accelerators
    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MAPVIEWER));
    // common controls (needed to turn on visual styles)
    InitCommonControls();
    // COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    _ASSERT(SUCCEEDED(hr));
    // Direct2D, multithreaded mode is important
    ComPtr<ID2D1Factory> pD2DFactory;
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, pD2DFactory.GetAddressOf());
    _ASSERT(SUCCEEDED(hr));
    // our WinInet wrapper
    HttpClient httpClient;

    // create and show a map window, set up to load basic OpenStreetMap, and center it
    // at a point at the outskirts of Smedsby village in Korsholm, Ostrobothnia region in Finland
    MapWindow mapWindow(httpClient, L"https://tile.openstreetmap.org", 256, pD2DFactory, hInstance);
    mapWindow.Create();
    mapWindow.Show(nCmdShow);
    mapWindow.Move(63.119671111, 21.712313611, 13);

    // kick of main message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}

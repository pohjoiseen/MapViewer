// MapWindow.cpp: MapWindow class implementation

#include "framework.h"
#include "Util.h"
#include "TileManager.h"
#include "Resource.h"
#include "MapWindow.h"

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

MapWindow::MapWindow(HttpClient& httpClient, std::wstring strBaseUrl, unsigned nTileSize, ComPtr<ID2D1Factory> pD2DFactory, HINSTANCE hInstance) : D2DWindow(pD2DFactory, hInstance),
    m_tileManager(httpClient, strBaseUrl, nTileSize, [=](Tile& tile) { Invalidate(); })
{
}

MapWindow::~MapWindow()
{
}

void MapWindow::Move(double dLat, double dLng, unsigned nZoom)
{
    _ASSERT(dLat >= -90.0 && dLat <= 90.0);
    _ASSERT(dLng >= -180.0 && dLat <= 180.0);
    _ASSERT(nZoom >= 0 && nZoom <= 18);
    m_dLat = dLat;
    m_dLng = dLng;
    m_nZoom = nZoom;
    UpdateView();
    Invalidate();
}

std::wstring MapWindow::WndClassName()
{
    return L"MapWindow";
}

void MapWindow::SetupWndClass(WNDCLASSEX &wndClass)
{
    // TODO: this should not be a top level window
    wndClass.hIcon = LoadIcon(hInstance(), MAKEINTRESOURCE(IDI_MAPVIEWER));
    wndClass.lpszMenuName = MAKEINTRESOURCEW(IDC_MAPVIEWER);
    wndClass.hIconSm = LoadIcon(hInstance(), MAKEINTRESOURCE(IDI_SMALL));
    wndClass.hbrBackground = nullptr;
}

void MapWindow::SetupCreateWindowParams(DWORD& dwStyle, DWORD& dwExStyle, std::wstring& strWindowName, int& x, int& y, int& width, int& height, HMENU& hmenu)
{
    // TODO: this should not be a top level window
	dwStyle = WS_OVERLAPPEDWINDOW;
	strWindowName = LoadStringFromResource(IDS_APP_TITLE);
}

LRESULT MapWindow::WndProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // TODO: this should not be a top level window -- these handlers are from the original
    // MSVC project template -- this window should be wrapped by some frame window instead
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        // Parse the menu selections:
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInstance(), MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd(), About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd());
            break;
        default:
            return Window::WndProc(uMsg, wParam, lParam);
        }
    }
    break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return D2DWindow::WndProc(uMsg, wParam, lParam);
    }
    return 0;
}

void MapWindow::EnsureRenderTarget()
{
    D2DWindow::EnsureRenderTarget();
    m_tileManager.SetRenderTarget(m_pRenderTarget);

    // [re]create brushes
    if (!m_pForegroundBrush) {
        m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(D2D1::ColorF::Black, .7f),
            m_pForegroundBrush.GetAddressOf()
        );
    }
    if (!m_pBackgroundBrush) {
        DWORD bgColor = GetSysColor(COLOR_3DFACE);
        m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(bgColor, .7f),
            m_pBackgroundBrush.GetAddressOf()
        );
    }
}

void MapWindow::InvalidateRenderTarget()
{
    D2DWindow::InvalidateRenderTarget();
    m_tileManager.InvalidateRenderTarget();
    m_pForegroundBrush.Reset();
    m_pBackgroundBrush.Reset();
}

void MapWindow::OnSize(unsigned nWidth, unsigned nHeight)
{
    D2DWindow::OnSize(nWidth, nHeight);
    UpdateView();
}

void MapWindow::OnLButtonDown(WORD wFlags, int x, int y)
{
    // start panning, storing the origin point for it
    m_bIsPanning = true;
    m_nPanningOriginX = x;
    m_nPanningOriginY = y;
    m_dPanningOriginLat = m_dLat;
    m_dPanningOriginLng = m_dLng;
}

void MapWindow::OnLButtonUp(WORD wFlags, int x, int y)
{
    m_bIsPanning = false;
}

void MapWindow::OnMouseWheel(WORD wFlags, int x, int y, int delta)
{
    // one WHEEL_DELTA corresponds to one zoom level
    delta /= WHEEL_DELTA;
    int zoom = m_nZoom + delta;
    // TODO: these should be configurable
    if (zoom < 0) {
        zoom = 0;
    }
    if (zoom > 18) {
        zoom = 18;
    }
    Move(m_dLat, m_dLng, (unsigned)zoom);
}

void MapWindow::OnMouseMove(WORD wFlags, int x, int y)
{
    // move map if we're currently in a panning mode (left mouse button pressed),
    // relative to the origin recorded when the button was pressed
    if (m_bIsPanning) {
        int xDiff = m_nPanningOriginX - x, yDiff = y - m_nPanningOriginY;        
        Move(m_dPanningOriginLat + yDiff * m_ldPixelSizeLat, m_dPanningOriginLng + xDiff * m_ldPixelSizeLng, m_nZoom);
    }
}

void MapWindow::OnPaintD2D()
{
    m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

    // get lat/lng of left top corner of the top left tile
    double n = std::pow(2, m_nZoom);
    long double lng = m_nTopLeftX / n * 360.0 - 180.0;
    long double latRad = std::atan(std::sinh(std::numbers::pi * (1.0 - 2.0 * m_nTopLeftY / n)));
    long double lat = latRad * 180.0 / std::numbers::pi;

    // difference in degrees with lat/lng of top left corner of the window
    long double lngDiff = lng - m_dTopLeftLng;
    long double latDiff = lat - m_dTopLeftLat;

    // difference in pixels
    int xOffset = (int)(lngDiff / m_ldPixelSizeLng);
    int yOffset = -(int)(latDiff / m_ldPixelSizeLat);

    // loop through tiles visible on screen
    for (unsigned y = 0; y <= m_nHeightInTiles; y++) {
        for (unsigned x = 0; x <= m_nWidthInTiles; x++) {
            // determine where the tile lands on screen
            int windowX = xOffset + x * m_tileManager.tileSize(), windowY = yOffset + y * m_tileManager.tileSize();
            D2D1_RECT_F rectangle = D2D1::RectF(windowX * 1.f, windowY * 1.f,
                windowX + m_tileManager.tileSize() * 1.f, windowY + m_tileManager.tileSize() * 1.f);

            // must be loaded
            Tile *tile = m_tileManager.GetTile({ x + m_nTopLeftX, y + m_nTopLeftY, m_nZoom });
            if (tile && tile->state() == TS_READY) {
                // draw the tile
                m_pRenderTarget->DrawBitmap(tile->d2dBitmap().Get(), rectangle);
            } else {
                // display not loaded tile (still loading or load error) as a background-colored rectable
                m_pRenderTarget->FillRectangle(rectangle, m_pBackgroundBrush.Get());
            }
        }
    }

    // draw "crosshairs" in the middle with the foreground brush
    D2D_SIZE_F size = m_pRenderTarget->GetSize();
    float dpiX, dpiY;
    m_pRenderTarget->GetDpi(&dpiX, &dpiY);
    dpiX /= 96.f;
    dpiY /= 96.f;
    m_pRenderTarget->DrawLine(
        D2D1::Point2F(std::round(size.width / 2.f), std::round(size.height / 2.f - 100.f * dpiY)),
        D2D1::Point2F(std::round(size.width / 2.f), std::round(size.height / 2.f + 100.f * dpiY)),
        m_pForegroundBrush.Get());
    m_pRenderTarget->DrawLine(
        D2D1::Point2F(std::round(size.width / 2.f - 100.f * dpiX), std::round(size.height / 2.f)),
        D2D1::Point2F(std::round(size.width / 2.f + 100.f * dpiX), std::round(size.height / 2.f)),
        m_pForegroundBrush.Get());
}

void MapWindow::UpdateView()
{
    // recalculate stuff
    UpdateNumbers();

    // remove invisible tiles
    m_tileManager.TrimTiles(m_nTopLeftX, m_nTopLeftY, m_nWidthInTiles, m_nHeightInTiles);

    // ensure all visible tiles are loaded
    for (unsigned y = m_nTopLeftY; y <= m_nTopLeftY + m_nHeightInTiles; y++) {
        for (unsigned x = m_nTopLeftX; x <= m_nTopLeftX + m_nWidthInTiles; x++) {
            m_tileManager.AddTile({ x, y, m_nZoom });
        }
    }
}

void MapWindow::UpdateNumbers()
{
    unsigned tileSize = m_tileManager.tileSize();

    // get window size in pixels
    RECT rect;
    bool result = GetClientRect(hWnd(), &rect);
    _ASSERT(result);
    unsigned windowWidth = rect.right, windowHeight = rect.bottom;

    // window size in tiles
    // add one to make sure we cover it
    m_nWidthInTiles = windowWidth / tileSize + 1,
    m_nHeightInTiles = windowHeight / tileSize + 1;

    // entire map width/height in tiles
    double n = std::pow(2, m_nZoom);

    // pixel size in map degrees
    // longitude is easy
    m_ldPixelSizeLng = 360.0 / n / tileSize;
    // latitude depends on where we are
    long double latRad = m_dLat * (std::numbers::pi / 180.0);
    m_ldPixelSizeLat = m_ldPixelSizeLng * std::cos(latRad);

    // top left corner map coords
    m_dTopLeftLng = m_dLng - (windowWidth / 2.0 * m_ldPixelSizeLng);
    m_dTopLeftLat = m_dLat + (windowHeight / 2.0 * m_ldPixelSizeLat);

    // top left corner tile coords
    m_nTopLeftX = (unsigned) std::floor(n * ((m_dTopLeftLng + 180.0) / 360.0));
    double topLeftLatRad = m_dTopLeftLat * (std::numbers::pi / 180.0);
    m_nTopLeftY = (unsigned) std::floor(n * (1.0 - (std::log(std::tan(topLeftLatRad) + 1.0 / std::cos(topLeftLatRad)) / std::numbers::pi)) / 2.0);
}

// TODO: remove, message handler for about box, from original MSVC project template
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

#pragma once

// MapWindow.h: a window that displays a map and reacts to interactions with it

#include "ComPtr.h"
#include "D2DWindow.h"

class TileManager;

class MapWindow : public D2DWindow
{
public:
	MapWindow(HttpClient& httpClient, std::wstring strBaseUrl, unsigned nTileSize, ComPtr<ID2D1Factory> pD2DFactory, HINSTANCE hInstance);
	~MapWindow();

	// Centers the map at a specified spot
	void Move(double dLat, double dLng, unsigned nZoom);

private:
	// Window setup and window procedure
	std::wstring WndClassName() override;
	void SetupWndClass(WNDCLASSEX &wndClass) override;
	void SetupCreateWindowParams(DWORD& dwStyle, DWORD& dwExStyle, std::wstring& strWindowName, int& x, int& y, int& width, int& height, HMENU& hmenu) override;
	LRESULT WndProc(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

	// Direct2D objects recreation
	void EnsureRenderTarget() override;
	void InvalidateRenderTarget() override;

	// Window message handlers
	void OnSize(unsigned nWidth, unsigned nHeight) override;
	void OnLButtonDown(WORD wFlags, int x, int y) override;
	void OnLButtonUp(WORD wFlags, int x, int y) override;
	void OnMouseWheel(WORD wFlags, int x, int y, int delta) override;
	void OnMouseMove(WORD wFlags, int x, int y) override;

	// Paint handler
	void OnPaintD2D() override;

	// source of tiles
	TileManager m_tileManager;

	// current coords
	double m_dLat = 0.0, m_dLng = 0.0;
	unsigned m_nZoom = 1;

	// various derived numbers
	// map coordinates of top left corner of the window
	double m_dTopLeftLat = 0.0, m_dTopLeftLng = 0.0;
	// tile coordinates of top left corner of the window
	unsigned m_nTopLeftX = 0, m_nTopLeftY = 0;
	// size of one screen pixel in map degrees
	long double m_ldPixelSizeLat = 0.0, m_ldPixelSizeLng = 0.0;
	// window size in tiles
	unsigned m_nWidthInTiles = 0, m_nHeightInTiles = 0;

	// tracking the map while panning
	bool m_bIsPanning = false;
	int m_nPanningOriginX = 0, m_nPanningOriginY = 0;
	double m_dPanningOriginLat = 0.0, m_dPanningOriginLng = 0.0;

	// drawing resources
	ComPtr<ID2D1SolidColorBrush> m_pForegroundBrush, m_pBackgroundBrush;

	// ensures tiles are loaded for the current view
	void UpdateView();
	// recalculates derived numbers above for the current view
	void UpdateNumbers();
};


#pragma once

// D2DWindow.h: base class for a window which paints itself using Direct2D API

#include "Window.h"
#include "ComPtr.h"

class D2DWindow : public Window
{
protected:
	// protected constructor, needs D2D factory
	D2DWindow(ComPtr<ID2D1Factory> pD2DFactory, HINSTANCE hInstance);

public:
	~D2DWindow() = 0;
	ID2D1HwndRenderTarget* renderTarget() const { return m_pRenderTarget.Get(); }

protected:
	ComPtr<ID2D1Factory> m_pD2DFactory;
	ComPtr<ID2D1HwndRenderTarget> m_pRenderTarget;

	void OnCreate() override;
	void OnSize(unsigned nWidth, unsigned nHeight) override;
	void OnPaint() override;

	// Function to actually repaint the (entire) window using Direct2D.
	// Render target is set up, no Begin/EndDraw() necessary here
	virtual void OnPaintD2D() = 0;

	// Creates and destroys Direct2D render target.  These are called as needed,
	// but if there are any long-lived assets attached to the render target (brushes,
	// bitmaps etc.), then derived classes must handle their creation (only if not exist)
	// and destruction (only if exist) in these functions.
	virtual void EnsureRenderTarget();
	virtual void InvalidateRenderTarget();

private:
	void CreateRenderTarget();
};


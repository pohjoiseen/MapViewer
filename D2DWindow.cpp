// D2DWindow.cpp: D2DWindow class implementation

#include "framework.h"
#include "D2DWindow.h"
#include "Util.h"

D2DWindow::D2DWindow(ComPtr<ID2D1Factory> pD2DFactory, HINSTANCE hInstance) : Window(hInstance),
    m_pD2DFactory(pD2DFactory)
{
}

D2DWindow::~D2DWindow()
{
    InvalidateRenderTarget();
}

void D2DWindow::OnCreate()
{
    EnsureRenderTarget();
}

void D2DWindow::OnSize(unsigned nWidth, unsigned nHeight)
{
    if (m_pRenderTarget) {
        // try to resize render target to window.
        // If we couldn't resize, release the device and we'll recreate it
        // during the next render pass.
        D2D1_SIZE_U size = D2D1::SizeU(nWidth, nHeight);
        if (FAILED(m_pRenderTarget->Resize(size))) {
            InvalidateRenderTarget();
        }
    }
}

void D2DWindow::OnPaint()
{
    // wrap painting with D2D render target
    PAINTSTRUCT ps;
    if (BeginPaint(hWnd(), &ps)) {
        EnsureRenderTarget();
        if (!m_pRenderTarget->CheckWindowState() & D2D1_WINDOW_STATE_OCCLUDED) {
            m_pRenderTarget->BeginDraw();
            OnPaintD2D();
            HRESULT hr = m_pRenderTarget->EndDraw();

            // in case of device loss, discard D2D render and force a repaint.
            // They will be re-create in the next pass
            if (hr == D2DERR_RECREATE_TARGET) {
                PrintLnDebug(L"D2DWindow::OnPaint render target needs to be recreated");
                InvalidateRenderTarget();
                InvalidateRect(hWnd(), nullptr, true);
            } else {
                _ASSERT(SUCCEEDED(hr));
            }
        }
        EndPaint(hWnd(), &ps);
    }
}

void D2DWindow::EnsureRenderTarget()
{
    if (!m_pRenderTarget) {
        CreateRenderTarget();
    }
}

void D2DWindow::InvalidateRenderTarget()
{
    m_pRenderTarget = ComPtr<ID2D1RenderTarget>();
}

void D2DWindow::CreateRenderTarget()
{
    // create default D2D render target properties
    D2D1_RENDER_TARGET_PROPERTIES renderTargetProperties = D2D1::RenderTargetProperties();

    // set the DPI to be the default system DPI to allow direct mapping
    // between image pixels and desktop pixels in different system DPI settings
    renderTargetProperties.dpiX = 96.f;
    renderTargetProperties.dpiY = 96.f;

    // set size to window size
    RECT rc;
    HRESULT hr = GetClientRect(hWnd(), &rc) ? S_OK : E_FAIL;
    _ASSERT(SUCCEEDED(hr));
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    // create
    hr = m_pD2DFactory->CreateHwndRenderTarget(
        renderTargetProperties,
        D2D1::HwndRenderTargetProperties(hWnd(), size),
        m_pRenderTarget.GetAddressOf()
    );
    _ASSERT(SUCCEEDED(hr));
}

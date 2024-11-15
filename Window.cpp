// Window.cpp: Window implementation

#include "framework.h"
#include "Window.h"

std::unordered_map<std::wstring, ATOM> Window::s_wndClasses;

Window::Window(HINSTANCE hInstance) : m_hInst(hInstance)
{
}

Window::~Window()
{
	if (m_hWnd) {
		DestroyWindow(m_hWnd);
	}
}

void Window::Create(Window& parent)
{
	Create(parent.hWnd());
}

void Window::Create(HWND hWndParent)
{
	EnsureWndClassRegistered();
	// default values for CreateWindowEx() can all be overridden by SetupCreateWindowParams()
	DWORD styleEx = 0, style = WS_CHILD;
	std::wstring windowName = std::wstring();
	int x = CW_USEDEFAULT, y = CW_USEDEFAULT, width = CW_USEDEFAULT, height = CW_USEDEFAULT;
	HMENU hMenu = nullptr;
	SetupCreateWindowParams(style, styleEx, windowName, x, y, width, height, hMenu);
	m_hWnd = CreateWindowEx(styleEx, WndClassName().c_str(), windowName.c_str(), style,
		x, y, width, height, hWndParent, hMenu, m_hInst, this);
	_ASSERT(m_hWnd);
}

void Window::Show(int nCmdShow)
{
	ShowWindow(m_hWnd, nCmdShow);
	UpdateWindow(m_hWnd);
}

void Window::Invalidate()
{
	if (m_hWnd) {
		InvalidateRect(m_hWnd, nullptr, true);
	}
}

void Window::SetupCreateWindowParams(DWORD& dwStyle, DWORD& dwExStyle, std::wstring& strWindowName, int& x, int& y, int& width, int& height, HMENU& hmenu)
{
}

LRESULT Window::WndProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
		OnCreate();
		return 0;

	case WM_SIZE:
		OnSize(LOWORD(lParam), HIWORD(lParam));
		return 0;

	case WM_LBUTTONDOWN:
		OnLButtonDown((WORD) wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

	case WM_LBUTTONUP:
		OnLButtonUp((WORD) wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

	case WM_MOUSEWHEEL:
		OnMouseWheel(GET_KEYSTATE_WPARAM(wParam), GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), GET_WHEEL_DELTA_WPARAM(wParam));
		return 0;

	case WM_MOUSEMOVE:
		OnMouseMove((WORD) wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

	case WM_PAINT:
		OnPaint();
		return 0;

	default:
		return DefWindowProc(m_hWnd, uMsg, wParam, lParam);
	}
}

void Window::OnCreate()
{
}

void Window::OnSize(unsigned nWidth, unsigned nHeight)
{
}

void Window::OnLButtonDown(WORD wFlags, int x, int y)
{
}

void Window::OnLButtonUp(WORD wFlags, int x, int y)
{
}

void Window::OnMouseWheel(WORD wFlags, int x, int y, int delta)
{
}

void Window::OnMouseMove(WORD wFlags, int x, int y)
{
}

void Window::OnPaint()
{
}

LRESULT Window::StaticWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// TODO: this misses initial WM_GETMINMAXINFO;
	// see https://devblogs.microsoft.com/oldnewthing/20191014-00/?p=102992
	// (dicussion in comments) for alternate method, which we don't need for now
	if (uMsg == WM_NCCREATE) {
		Window* pWindow = reinterpret_cast<Window*>(reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams);
		// store HWND ASAP, as more messages arrive even before CreateWindowEx() returns
		pWindow->m_hWnd = hWnd;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWindow));
	}

	LONG_PTR pWindow = GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (pWindow) {
		return reinterpret_cast<Window*>(pWindow)->WndProc(uMsg, wParam, lParam);
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void Window::EnsureWndClassRegistered()
{
	std::wstring wndClassName = WndClassName();
	if (!s_wndClasses.contains(wndClassName)) {
		// default window class values, may all be overridden by SetupWndClass()
		WNDCLASSEX wcex;
		wcex.cbSize = sizeof(WNDCLASSEX);
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpszClassName = wndClassName.c_str();
		wcex.lpfnWndProc = StaticWndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = m_hInst;
		wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		SetupWndClass(wcex);
		ATOM atom = RegisterClassEx(&wcex);
		_ASSERT(atom);
		s_wndClasses[wndClassName] = atom;
	}
}

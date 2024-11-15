#pragma once

// Window.h: base Window (HWND, WndProc) wrapper class
// Not an all-purpose one, just with features relevant to this project.

class Window
{
public:
	Window(HINSTANCE hInst);

	// no copy/assignment
	Window& operator=(const Window &) = delete;
	Window(const Window &) = delete;

	virtual ~Window() = 0;

	void Create(Window &parent);
	void Create(HWND hWndParent = nullptr);
	void Show(int nCmdShow);
	void Invalidate();

	HINSTANCE hInstance() const { return m_hInst; }
	HWND hWnd() const { return m_hWnd; }

protected:
	// The next three functions should normally be always overridden
	// TODO: probably better way to handle WndClassName() (something something constexpr?)
	virtual std::wstring WndClassName() = 0;
	virtual void SetupWndClass(WNDCLASSEX &wndClass) = 0;
	virtual void SetupCreateWindowParams(DWORD& dwStyle, DWORD& dwExStyle, std::wstring& strWindowName, int& x, int& y, int& width, int& height, HMENU& hmenu);

	// Base window procedure, may be overridden
	virtual LRESULT WndProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

	// Common WM_XXX handlers
	virtual void OnCreate();
	virtual void OnSize(unsigned nWidth, unsigned nHeight);
	virtual void OnLButtonDown(WORD wFlags, int x, int y);
	virtual void OnLButtonUp(WORD wFlags, int x, int y);
	virtual void OnMouseWheel(WORD wFlags, int x, int y, int delta);
	virtual void OnMouseMove(WORD wFlags, int x, int y);
	virtual void OnPaint();

private:
	HINSTANCE m_hInst;
	HWND m_hWnd = nullptr;

	// Win32 window classes are registered as needed and stored here
	static std::unordered_map<std::wstring, ATOM> s_wndClasses;

	void EnsureWndClassRegistered();

	// Common static window procedure.  Pointer to class instance stored in GWLP_USERDATA,
	// execution is routed to WndProc() through it
	static LRESULT StaticWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

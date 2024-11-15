# MapViewer

This is my fun little side project, the point of which is to write a very basic map (at least OpenStreetMap)
viewer application using nothing but C++ and bare Win32 API.  This includes C++ standard library (latest
supported by MSVC), but excludes something like MFC/ATL/WTL.  Honestly it's not that bad experience, once
you make some kind of wrappers for the oldest and/or lowest-level Windows APIs.  But I've only ever did
commercial work as a web developer and haven't touched C++ and Win32 since I was a teenager (15-20ish years
ago), so if someone is reading this I hope you'll excuse me if I'm doing anything particularly stupid :)  

As of now (15.11.2024) it can load OpenStreetMap and allows you to move around and zoom via mouse, but that's
basically it.  **It is somewhat buggy, in particular zooming/unzooming quickly, over several levels, or trying
to go to the lowest zoom level (entire world visible) crashes.**

The project is written using MSVC 2022 Community, should build cleanly with it and should not build with anything
else.  I set API compatibility level to Windows 7 but haven't tested in anything else than my Windows 11.
The license is MIT.

## Implementation

There are a few useful Win32 APIs for this kind of application.  **WinInet** is an HTTP (and FTP) client,
built into Windows.  In practice it's just the HTTP/FTP backend for Internet Explorer, and as such it's a quite
old component, introduced all the way back in MSIE 3, although probably that version didn't support all of its more
modern APIs.  Unlike e. g. curl it can fully transparently cache downloaded files on disk, which is
a feature greatly useful for a map viewer, which needs to download and redownload a large number of tiles;
basically it frees us from having to keep our own cache, which would have been almost mandatory.

WinInet can be used in asynchronous mode, which we of course need to do to download many tiles at the same time
without blocking UI, but it makes the code a lot gnarlier.  We use a simple wrapper (`HttpClient` class)
which hides the ugly details and can accept a callback argument, making this look as straightforward as
JS.  A crucial difference though is that callbacks happen on worker threads, and accessing stuff from the
main thread needs synchronization.  This is not a huge problem in this case; we create Direct2D bitmaps
in these worker threads but Direct2D can be used in multithreaded mode, and then you don't need to do anything
extra about it.

The standard way to serve raster map tiles over HTTP is in 256x256 PNGs, so we need to handle PNG images.
This is where **WIC** (Windows Imaging Component) comes into play.  It's a COM-based library which, well,
loads images.  We really don't need to do anything fancy with it, just decode a memory stream into a bitmap,
which can all happen in the callback on the worker thread.  WIC was introduced in Vista and was backported to
Windows XP.

And for actually painting the tiles we use **Direct2D**, which is the modern Windows graphics API, introduced
in Windows 7 and backported to Vista.  It is a 2D API built over Direct3D, and as such should always be
hardware-accelerated (when rendering to screen).  The API, so far as I can tell, is not greatly different
from classic GDI or GDI+.  One difference is that we should be able to gracefull handle device (render target)
lost conditions, which means recreating all resources associated with that render target; there's a helper
`D2DWindow` base class to handle that.

In our case the resources are mostly tile bitmaps; Direct2D handily includes a function to load them
directly from WIC objects.  These bitmaps should live already in VRAM, so there's no need to keep tiles
around in main RAM after that.  We discard tiles that are no longer visible (after map movement or window
resizing), releasing these bitmaps; we keep around only a small cache of no longer visible tiles (equal
in number to the number of visible tiles), the latest ones loaded, so that minor adjustments to the map
don't necessarily trigger tile reloads.  Overall the "backend" that loads and keeps track of tiles is in
`TileManager` class, and the "frontend" that shows correct tiles in correct locations and reacts to user
actions is in `MapWindow` class.

The rest of the code so far is relatively straightforward; we use a base `Window` class to wrap HWND/WndProc,
and a COM smart pointer `ComPtr` class, to work with Windows components (WIC is COM-based and Direct2D
is COM-like).  The latter class I just lifted verbatim from an MSDN Magazine article, but everything else
is cobbled together by me :)

Written by Alexander Ulyanov <procyonar@gmail.com>
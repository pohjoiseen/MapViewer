#pragma once

// TileManager.h: class responsible for fetching map tiles from the Internet,
// loading them into Direct2D bitmaps, and keeping around as needed.
// Uses our HttpClient class for HTTP requests and WIC (Windows Imaging Component) API
// to decode tile images (PNGs) into bitmaps.
// One TileManager is meant to be used by one MapWindow

#include "ComPtr.h"

class Tile;
class HttpClient;
struct TileCoords;

class TileManager
{
public:
	// callback type to call on a successful tile load
	typedef std::function<void(Tile& tile)> OnTileLoadedCallback;

	// strBaseUrl should be a base URL (without trailing slash) for tiles in
	// the standard .../{ZOOM}/{X}/{Y}.png layout.  nTileSize is in pixels (tiles must be square)
	TileManager(HttpClient& httpClient, std::wstring strBaseUrl, unsigned nTileSize,
		OnTileLoadedCallback fnTileLoadedCallback);
	~TileManager();

	// Direct2D render target set/reset.  Tiles are loaded into Direct2D bitmaps,
	// which must be attached to a valid render target.  Invalidating render target
	// deletes all loaded tiles and means any newly loaded tiles will just be discarded
	void SetRenderTarget(ComPtr<ID2D1RenderTarget> pRenderTarget);
	void InvalidateRenderTarget();

	// gets URL for certain tile coords
	std::wstring GetTileURL(TileCoords coords);

	// tries to load a tile with given coords, kicking of HTTP request
	// if a tile is alread loaded, does nothing
	// if a tile failed to load previously (TS_ERROR) state, reloads
	Tile& AddTile(TileCoords coords);

	// gets a tile at give coords, if it exists, and null otherwise
	Tile* GetTile(TileCoords coords);

	// removes unnecessary tiles outside of the specified window
	// in practice, keeps some more tiles in case user moves back
	void TrimTiles(unsigned x, unsigned y, unsigned width, unsigned height);
	
	unsigned tileSize() const { return m_nTileSize; }

private:
	HttpClient& m_httpClient;
	// base WIC component to decode images
	ComPtr<IWICImagingFactory> m_pWICFactory;
	std::wstring m_strBaseUrl;
	unsigned m_nTileSize;
	// URL -> tile map
	std::unordered_map<std::wstring, Tile> m_mapTiles;
	OnTileLoadedCallback m_fnTileLoadedCallback;
	ComPtr<ID2D1RenderTarget> m_pRenderTarget;

	void LoadTile(Tile& tile);

	// callback for HttpClient
	void LoadTileCallback(Tile &tile, int nStatus, void* pBuffer, size_t sizeLength);
};

enum TileState
{
	TS_LOADING = 0, // HTTP request in progress
	TS_READY = 1,	// loaded successfully
	TS_ERROR = 2	// failed to load for whatever reason
};

struct TileCoords
{
	unsigned x;
	unsigned y;
	unsigned zoom;
	TileCoords(unsigned x, unsigned y, unsigned zoom) : x(x), y(y), zoom(zoom) {}
};

// Class wrapping a single tile, which might be in a loaded state (with attached bitmap) or not.
// Holds also a timestamp of instantiation, which helps TileManager::TrimTiles() to remove the "oldest" tiles.
class Tile
{
public:
	Tile(TileCoords coords, std::wstring strUrl);
	~Tile();

	unsigned x() const { return m_coords.x; }
	unsigned y() const { return m_coords.y; }
	unsigned zoom() const { return m_coords.zoom; }
	const std::wstring& url() const { return m_strUrl; }
	TileState state() const { return m_state; }
	ComPtr<ID2D1Bitmap> d2dBitmap() const { return m_pD2dBitmap; }
	long created() const { return m_tmCreated; }

private:
	friend class TileManager;

	TileCoords m_coords;
	std::wstring m_strUrl;
	TileState m_state;
	ComPtr<ID2D1Bitmap> m_pD2dBitmap;
	long m_tmCreated;
};

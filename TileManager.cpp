// TileManager.cpp: TileManager class implementation

#include "framework.h"
#include "Util.h"
#include "HttpClient.h"
#include "D2DWindow.h"
#include "TileManager.h"

TileManager::TileManager(HttpClient& httpClient, std::wstring strBaseUrl, unsigned nTileSize, OnTileLoadedCallback fnTileLoadedCallback)
	: m_httpClient(httpClient), m_strBaseUrl(strBaseUrl), m_nTileSize(nTileSize), m_fnTileLoadedCallback(fnTileLoadedCallback)
{
	HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory,
		nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(m_pWICFactory.GetAddressOf()));
	_ASSERT(SUCCEEDED(hr));
}

TileManager::~TileManager()
{
}

void TileManager::SetRenderTarget(ComPtr<ID2D1RenderTarget> pRenderTarget)
{
	if (!m_pRenderTarget || m_pRenderTarget.Get() != pRenderTarget.Get()) {
		InvalidateRenderTarget();
		m_pRenderTarget = pRenderTarget;
	}
}

void TileManager::InvalidateRenderTarget()
{
	// remove all tiles that are already loaded
	std::erase_if(m_mapTiles, [](auto kv) { return kv.second.state() == TS_READY; });
	m_pRenderTarget.Reset();
}

std::wstring TileManager::GetTileURL(TileCoords coords)
{
	return std::format(L"{}/{}/{}/{}.png", m_strBaseUrl, coords.zoom, coords.x, coords.y);
}

Tile& TileManager::AddTile(TileCoords coords)
{
	// create a Tile instance, if not exists
	// if already exists and its state is not error, then don't need to do anything;
	// otherwise kick off loading
	std::wstring url = GetTileURL(coords);
	auto [pos, success] = m_mapTiles.try_emplace(url, coords, url);
	if (success || pos->second.state() == TS_ERROR) {
		LoadTile(pos->second);
	}
	return pos->second;
}

Tile* TileManager::GetTile(TileCoords coords)
{
	std::wstring url = GetTileURL(coords);
	auto tile = m_mapTiles.find(url);
	if (tile != m_mapTiles.end()) {
		return &tile->second;
	}
	return nullptr;
}

void TileManager::TrimTiles(unsigned x, unsigned y, unsigned width, unsigned height)
{
	// get all tiles which are not currently displayed
	std::vector<std::pair<std::wstring, Tile*>> deleteCandidates;
	for (auto& kv : m_mapTiles) {
		if (kv.second.x() < x || kv.second.x() > x + width ||
			kv.second.y() < y || kv.second.y() > y + height) {
			deleteCandidates.emplace_back(kv.first, &kv.second);
		}
	}

	// sort by age, newest first
	std::sort(deleteCandidates.begin(), deleteCandidates.end(),
		[](std::pair<std::wstring, Tile*> kv1, std::pair<std::wstring, Tile*> kv2) { return kv1.second->created() > kv2.second->created(); });

	// keep some cache of newest invisible tiles, equal to the number of already displayed tiles
	unsigned keep = width * height;

	// and delete the rest, if there are any
	if (deleteCandidates.size() > keep) {
		for (auto p = deleteCandidates.begin() + (width * height); p < deleteCandidates.end(); p++) {
			m_mapTiles.erase(p->first);
		}
	}
}

void TileManager::LoadTile(Tile& tile)
{
	tile.m_state = TS_LOADING;
	// start HTTP download, everything else happens asyncronously in callback
	m_httpClient.Get(tile.m_strUrl, [&](int nStatus, void* pBuffer, size_t szLength) {
		LoadTileCallback(tile, nStatus, pBuffer, szLength);
	});
}

void TileManager::LoadTileCallback(Tile &tile, int nStatus, void* pBuffer, size_t sizeLength)
{
	// this is a callback executing on a different (worker) thread!
	// this should be working fine as long as Direct2D is initialized in multithread mode,
	// since we're accessing it here across threads

	if (pBuffer) {
		// can't do much if no render target exists right now
		if (!m_pRenderTarget) {
			OutputDebugString(L"Tile loaded but no render target, discarding");
			delete[] pBuffer;
			tile.m_state = TS_ERROR;
			return;
		}

		// will use a bunch of objects from WIC and then a target bitmap
		HRESULT hr;
		ComPtr<IStream> pStream;
		ComPtr<IWICBitmapDecoder> pDecoder;
		ComPtr<IWICBitmapFrameDecode> pFrame;
		ComPtr<IWICFormatConverter> pConverter;
		ComPtr<ID2D1Bitmap> pBitmap;

		// wrap buffer into an IStream which WIC expects
		pStream.Attach(SHCreateMemStream(reinterpret_cast<BYTE*>(pBuffer), (UINT)sizeLength));
		_ASSERT(pStream.Get());

		// some straightforward WIC stuff, just keep track of errors at every stip
		hr = m_pWICFactory->CreateDecoderFromStream(pStream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, pDecoder.GetAddressOf());
		if (SUCCEEDED(hr)) {
			hr = pDecoder->GetFrame(0, pFrame.GetAddressOf());
			if (SUCCEEDED(hr)) {
				hr = m_pWICFactory->CreateFormatConverter(pConverter.GetAddressOf());
				_ASSERT(SUCCEEDED(hr));  // surely cannot fail
				hr = pConverter->Initialize(
					pFrame.Get(),                    // Input bitmap to convert
					GUID_WICPixelFormat32bppPBGRA,   // Destination pixel format
					WICBitmapDitherTypeNone,         // Specified dither pattern
					nullptr,                         // Specify a particular palette 
					0.f,                             // Alpha threshold
					WICBitmapPaletteTypeCustom       // Palette translation type
				);
				if (SUCCEEDED(hr)) {
					hr = m_pRenderTarget->CreateBitmapFromWicBitmap(pConverter.Get(), pBitmap.GetAddressOf());
					if (!SUCCEEDED(hr)) {
						PrintLnDebug(L"Failed to create D2D bitmap for buffer 0x{:x}, HRESULT = {}", (intptr_t)pBuffer, (intptr_t)hr);
					}
				} else {
					PrintLnDebug(L"Failed to initialize converter for buffer 0x{:x}, HRESULT = {}\n", (intptr_t)pBuffer, (intptr_t)hr);
				}
			} else {
				PrintLnDebug(L"Failed to retrieve frame for buffer 0x{:x}, HRESULT = {}\n", (intptr_t)pBuffer, (intptr_t)hr);
			}
		} else {
			PrintLnDebug(L"Failed to create decoder for buffer 0x{:x}, HRESULT = {}\n", (intptr_t)pBuffer, (intptr_t)hr);
		}

		// at this point we either have a loaded bitmap or not, if errored out along the way
		// discard the original buffer either way
		delete[] pBuffer;
		if (pBitmap.Get()) {
			tile.m_state = TS_READY;
			tile.m_pD2dBitmap = pBitmap;
			m_fnTileLoadedCallback(tile);
		} else {
			tile.m_state = TS_ERROR;
		}
	} else {
		PrintLnDebug(L"Downloading tile {} failed: nStatus = {}\n", tile.m_strUrl, nStatus);
		tile.m_state = TS_ERROR;
	}
}

Tile::Tile(TileCoords coords, std::wstring strUrl)
	: m_coords(coords), m_state(TS_LOADING), m_strUrl(strUrl)
{
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	m_tmCreated = (((long long) ft.dwHighDateTime) << 32) + ft.dwLowDateTime;
}

Tile::~Tile()
{
}
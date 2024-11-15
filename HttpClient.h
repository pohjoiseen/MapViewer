#pragma once

// HttpClient.h: simple asynchronous HTTP client class, wrapping WinInet API.
// For the purposes of this project we only need to make simple GET requests (for map tile images)
// without any customization, but we need to make a lot of them.  Very conveniently WinInet has
// built-in caching support, which is exactly what we want for fetching tiles.
// No more than one instance per app should be necessary.

class HttpClient
{
public:
	HttpClient();
	~HttpClient();

	// Callback type
	// nStatus = 0: request successful (2xx status, do not distinguish between them), pBuffer/szLength contain response
	// nStatus > 0: server reported error, nStatus equals error code.  pBuffer is null (do not save response in this case)
	// nStatus < 0: error making request, nStatus equals -(INTERNET_ASYNC_RESULT::dwError).  pBuffer is null
	typedef std::function<void(int nStatus, void *pBuffer, size_t szLength)> OnFinishCallback;

	// Main/only entry point, only gets an URL to fetch and a callback to call when the request is finished
	// (whether successfully or not).  Note that the callback will execute on a different, worker thread!
	void Get(std::wstring strUrl, OnFinishCallback fnOnFinish);

private:
	// base WinInet handle
	HINTERNET m_hInternet;

	// structure for keeping track of a request
	struct HttpRequest
	{
		HttpClient& client;
		std::wstring strUrl;
		OnFinishCallback fnOnFinish;
		HINTERNET hRequest = nullptr;
		INTERNET_BUFFERS buffers = { sizeof(INTERNET_BUFFERS) };
		void* pBuffer = nullptr;
		size_t sizeLength = 0;

		HttpRequest(HttpClient& client, std::wstring strUrl, OnFinishCallback fnOnFinish) :
			client(client), strUrl(strUrl), fnOnFinish(fnOnFinish) {}
	};

	// async callback, dwContext should be a pointer to HttpRequest
	static void StaticInternetStatusCallback(
		HINTERNET hInternet,
		DWORD_PTR dwContext,
		DWORD dwInternetStatus,
		LPVOID lpvStatusInformation,
		DWORD dwStatusInformationLength
	);

	// wrapped callback
	void InternetStatusCallback(
		HttpRequest &request,
		HINTERNET hInternet,
		DWORD dwInternetStatus,
		LPVOID lpvStatusInformation,
		DWORD dwStatusInformationLength
	);

	// stop a request (if active) and delete HttpRequest structure
	void Terminate(HINTERNET hRequest, bool bKeepBuffer = false);

	// currently active requests, and a mutex to sync changes to it
	std::vector<std::unique_ptr<HttpRequest>> m_vecRequests;
	std::mutex m_vecRequestsMutex;
};


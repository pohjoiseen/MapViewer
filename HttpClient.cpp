// HttpClient.cpp: HttpClient class implementation

#include "framework.h"
#include "Util.h"
#include "HttpClient.h"

// TODO: should we pretend to be a browser?
static const wchar_t USER_AGENT[] = L"MapViewer/0.1";

HttpClient::HttpClient()
{
	// open main WinInet handle for async requests and all defaults, and set a callback for it
	m_hInternet = InternetOpen(USER_AGENT, INTERNET_OPEN_TYPE_PRECONFIG,
		nullptr, nullptr, INTERNET_FLAG_ASYNC);
	_ASSERT(m_hInternet);
	InternetSetStatusCallback(m_hInternet, StaticInternetStatusCallback);
}

HttpClient::~HttpClient()
{
	InternetCloseHandle(m_hInternet);
}

void HttpClient::Get(std::wstring strUrl, OnFinishCallback fnOnFinish)
{
	// create a new HttpRequest instance to track request
	std::lock_guard lock(m_vecRequestsMutex);
	std::unique_ptr<HttpRequest> &pRequest = m_vecRequests.emplace_back(new HttpRequest(*this, strUrl, fnOnFinish));

	// fire off async request, using all default settings.  May already return request handle or may
	// result in ERROR_IO_PENDING, in which case the callback needs to save the handle later
	HINTERNET hRequest = InternetOpenUrl(m_hInternet, strUrl.c_str(), nullptr, 0, 0, reinterpret_cast<DWORD_PTR>(pRequest.get()));
	if (hRequest) {
		pRequest->hRequest = hRequest;
	} else {
		_ASSERT(GetLastError() == ERROR_IO_PENDING);
	}
}

void HttpClient::Terminate(HINTERNET hRequest, bool bKeepBuffer)
{
	// find a matching HttpRequest by its HINTERNET
	std::lock_guard lock(m_vecRequestsMutex);
	auto it = std::find_if(m_vecRequests.begin(), m_vecRequests.end(),
		[=](auto& request) { return request.get() && request->hRequest == hRequest; });
	if (it != m_vecRequests.end()) {
		// close its subhandle, delete response buffer if present and request, and remove request from the list
		auto request = it->get();
		InternetCloseHandle(hRequest);
		if (!bKeepBuffer && request->pBuffer) {
			delete[] request->pBuffer;
		}
		m_vecRequests.erase(std::remove(m_vecRequests.begin(), m_vecRequests.end(), *it), m_vecRequests.end());
	}
}

void __stdcall HttpClient::StaticInternetStatusCallback(HINTERNET hInternet, DWORD_PTR dwContext, DWORD dwInternetStatus, LPVOID lpvStatusInformation, DWORD dwStatusInformationLength)
{
	HttpRequest* pRequest = reinterpret_cast<HttpRequest*>(dwContext);
	pRequest->client.InternetStatusCallback(*pRequest, hInternet, dwInternetStatus, lpvStatusInformation, dwStatusInformationLength);
}

void HttpClient::InternetStatusCallback(HttpRequest &request, HINTERNET hInternet, DWORD dwInternetStatus, LPVOID lpvStatusInformation, DWORD dwStatusInformationLength)
{
	//PrintLnDebug(L"InternetStatusCallback HINTERNET = 0{:x} dwInternetStatus = {}", (intptr_t) hInternet, dwInternetStatus);

	switch (dwInternetStatus)
	{
	case INTERNET_STATUS_HANDLE_CREATED: {
		// store request handle in HttpRequest
		INTERNET_ASYNC_RESULT* pResult = reinterpret_cast<INTERNET_ASYNC_RESULT*>(lpvStatusInformation);
		request.hRequest = reinterpret_cast<HINTERNET>(pResult->dwResult);
		break;
	}

	case INTERNET_STATUS_REQUEST_COMPLETE: {
		INTERNET_ASYNC_RESULT* pResult = reinterpret_cast<INTERNET_ASYNC_RESULT*>(lpvStatusInformation);
		if (pResult->dwResult) {
			// successul callback

			// response buffer not yet allocated, must be a first callback
			if (!request.buffers.dwBufferTotal) {
				DWORD dw = 0;
				DWORD dwLength = sizeof(dw);
				DWORD dwIndex = 0;

				// check HTTP status
				bool statusCodeRetrieved = HttpQueryInfo(request.hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &dw, &dwLength, &dwIndex);
				_ASSERT(statusCodeRetrieved);
				// allow only 2xx status, otherwise bail out and don't bother reading response
				if (dw < 200 || dw > 299) {
					request.fnOnFinish(dw, nullptr, (size_t)0);
					Terminate(request.hRequest);
					return;
				}

				// TODO: assume Content-Length header exists, which should be the case for our use but not in general.  Assert out if it doesn't
				bool contentLengthRetrieved = HttpQueryInfo(request.hRequest, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &dw, &dwLength, &dwIndex);
				_ASSERT(contentLengthRetrieved);

				// allocate buffer and set up INTERNET_BUFFERS according to Content-Length received
				request.pBuffer = new char[dw];
				request.buffers.lpvBuffer = request.pBuffer;
				request.sizeLength = dw;
				request.buffers.dwBufferTotal = dw;
				request.buffers.dwBufferLength = dw;
			}

			if (request.buffers.dwBufferLength > 0) {
				// dwBufferLength > 0 --> need to issue a read request (first or subsequent)

				// this condition is true in case there was a read but it was incomplete, in that case need to adjust
				// pointer and length in INTERNET_BUFFERS to point to the part that's not yet read
				if (request.buffers.dwBufferLength < request.buffers.dwBufferTotal) {
					request.buffers.lpvBuffer =
						reinterpret_cast<char*>(request.buffers.lpvBuffer) + request.buffers.dwBufferLength;
					size_t downloadedBytes =
						reinterpret_cast<char*>(request.buffers.lpvBuffer) -
						reinterpret_cast<char*>(request.pBuffer);
					request.buffers.dwBufferLength = (DWORD) (request.sizeLength - downloadedBytes);
				}

				// issue read request
				bool readSuccess = InternetReadFileEx(request.hRequest, &request.buffers, IRF_ASYNC, reinterpret_cast<DWORD_PTR>(&request));
				if (readSuccess) {
					// despite async mode it can actually return already synchronously anyway
					request.fnOnFinish(0, request.pBuffer, request.sizeLength);
					Terminate(request.hRequest, true);
				} else {
					// otherwise expect ERROR_IO_PENDING, any other error is an acual error
					unsigned error = GetLastError();
					if (error != ERROR_IO_PENDING) {
						request.fnOnFinish(-(int)pResult->dwError, nullptr, (size_t)0);
						Terminate(request.hRequest);
						return;
					}
				}
			} else {
				// last dwBufferLength == 0 -> all read successfully
				request.fnOnFinish(0, request.pBuffer, request.sizeLength);
				Terminate(request.hRequest, true);
			}
		} else {
			// error callback, report and wrap up
			request.fnOnFinish(-(int)pResult->dwError, nullptr, (size_t)0);
			Terminate(request.hRequest);
		}
		break;
	}

	case INTERNET_STATUS_HANDLE_CLOSING: {
		//Terminate(request.hRequest);  // might already be done
		break;
	}
	}
}

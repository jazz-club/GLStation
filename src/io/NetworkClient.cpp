#include "io/NetworkClient.hpp"
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>

namespace GLStation::Util {

NetworkClient::NetworkClient() : m_handle(nullptr) {
	m_handle = WinHttpOpen(L"GLStation", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
						   WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
}

NetworkClient::~NetworkClient() {
	if (m_handle)
		WinHttpCloseHandle((HINTERNET)m_handle);
}

NetworkResponse NetworkClient::post(const std::string &url,
									const std::string &body) {
	NetworkResponse response = {false, "", 0, ""};
	if (!m_handle) {
		response.errorMessage = "WinHttp uninitialised";
		return response;
	}

	wchar_t wUrl[2048];
	MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, wUrl, 2048);
	URL_COMPONENTS urlComp = {};
	urlComp.dwStructSize = sizeof(urlComp);
	urlComp.dwHostNameLength = (DWORD)-1;
	urlComp.dwUrlPathLength = (DWORD)-1;
	urlComp.dwExtraInfoLength = (DWORD)-1;
	if (!WinHttpCrackUrl(wUrl, 0, 0, &urlComp)) {
		response.errorMessage = "Failed to parse";
		return response;
	}
	std::wstring wHost(urlComp.lpszHostName, urlComp.dwHostNameLength);
	std::wstring wPath(urlComp.lpszUrlPath,
					   urlComp.dwUrlPathLength + urlComp.dwExtraInfoLength);

	HINTERNET hConnect =
		WinHttpConnect((HINTERNET)m_handle, wHost.c_str(), urlComp.nPort, 0);
	if (!hConnect) {
		response.errorMessage = "Failed to connect";
		return response;
	}

	HINTERNET hRequest = WinHttpOpenRequest(
		hConnect, L"POST", wPath.c_str(), NULL, WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES,
		(urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
	if (!hRequest) {
		WinHttpCloseHandle(hConnect);
		response.errorMessage = "Failed to open request";
		return response;
	}

	DWORD timeout = 30000;
	WinHttpSetOption(hRequest, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout,
					 sizeof(timeout));
	bool ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
								 (LPVOID)body.c_str(), (DWORD)body.length(),
								 (DWORD)body.length(), 0);
	if (ok)
		ok = WinHttpReceiveResponse(hRequest, NULL);

	if (ok) {
		DWORD dwCode = 0, dwSize = sizeof(dwCode);
		WinHttpQueryHeaders(
			hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX, &dwCode, &dwSize,
			WINHTTP_NO_HEADER_INDEX);
		response.statusCode = dwCode;
		std::string out;
		DWORD dwRead = 0;
		do {
			dwSize = 0;
			if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
				break;
			if (dwSize == 0)
				break;
			std::vector<char> buf(dwSize + 1, 0);
			if (!WinHttpReadData(hRequest, buf.data(), dwSize, &dwRead))
				break;
			out.append(buf.data(), dwRead);
		} while (dwSize > 0);
		response.body = std::move(out);
		response.success = (response.statusCode == 200);
	}
	WinHttpCloseHandle(hRequest);
	WinHttpCloseHandle(hConnect);
	return response;
}

NetworkResponse NetworkClient::get(const std::string &url) {
	NetworkResponse response = {false, "", 0, ""};
	if (!m_handle) {
		response.errorMessage = "WinHttp uninitialised";
		return response;
	}

	wchar_t wUrl[2048];
	MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, wUrl, 2048);
	URL_COMPONENTS urlComp = {};
	urlComp.dwStructSize = sizeof(urlComp);
	urlComp.dwHostNameLength = (DWORD)-1;
	urlComp.dwUrlPathLength = (DWORD)-1;
	urlComp.dwExtraInfoLength = (DWORD)-1;
	if (!WinHttpCrackUrl(wUrl, 0, 0, &urlComp)) {
		response.errorMessage = "Failed to parse";
		return response;
	}
	std::wstring wHost(urlComp.lpszHostName, urlComp.dwHostNameLength);
	std::wstring wPath(urlComp.lpszUrlPath,
					   urlComp.dwUrlPathLength + urlComp.dwExtraInfoLength);

	HINTERNET hConnect =
		WinHttpConnect((HINTERNET)m_handle, wHost.c_str(), urlComp.nPort, 0);
	if (!hConnect) {
		response.errorMessage = "Failed to connect";
		return response;
	}

	HINTERNET hRequest = WinHttpOpenRequest(
		hConnect, L"GET", wPath.c_str(), NULL, WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES,
		(urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
	if (!hRequest) {
		WinHttpCloseHandle(hConnect);
		response.errorMessage = "Failed to open request";
		return response;
	}

	DWORD timeout = 30000;
	WinHttpSetOption(hRequest, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout,
					 sizeof(timeout));
	WinHttpAddRequestHeaders(hRequest, L"User-Agent: GLStation\r\n", (DWORD)-1,
							 WINHTTP_ADDREQ_FLAG_ADD);
	bool ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
								 WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
	if (ok)
		ok = WinHttpReceiveResponse(hRequest, NULL);

	if (ok) {
		DWORD dwCode = 0, dwSize = sizeof(dwCode);
		WinHttpQueryHeaders(
			hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX, &dwCode, &dwSize,
			WINHTTP_NO_HEADER_INDEX);
		response.statusCode = dwCode;
		std::string out;
		DWORD dwRead = 0;
		do {
			dwSize = 0;
			if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
				break;
			if (dwSize == 0)
				break;
			std::vector<char> buf(dwSize + 1, 0);
			if (!WinHttpReadData(hRequest, buf.data(), dwSize, &dwRead))
				break;
			out.append(buf.data(), dwRead);
		} while (dwSize > 0);
		response.body = std::move(out);
		response.success = (response.statusCode == 200);
	}
	WinHttpCloseHandle(hRequest);
	WinHttpCloseHandle(hConnect);
	return response;
}

} // namespace GLStation::Util

#else
#include <curl/curl.h>

namespace GLStation::Util {

static size_t writeCallback(char *ptr, size_t size, size_t nmemb,
							void *userdata) {
	size_t total = size * nmemb;
	std::string *out = static_cast<std::string *>(userdata);
	out->append(ptr, total);
	return total;
}

NetworkClient::NetworkClient() : m_handle(nullptr) {
	curl_global_init(CURL_GLOBAL_DEFAULT);
	m_handle = curl_easy_init();
}

NetworkClient::~NetworkClient() {
	if (m_handle) {
		curl_easy_cleanup(static_cast<CURL *>(m_handle));
		m_handle = nullptr;
	}
	curl_global_cleanup();
}

NetworkResponse NetworkClient::get(const std::string &url) {
	NetworkResponse response = {false, "", 0, ""};
	if (!m_handle) {
		response.errorMessage = "curl uninitialised";
		return response;
	}
	CURL *curl = static_cast<CURL *>(m_handle);
	std::string responseBody;
	curl_easy_reset(curl);
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "GLStation");
	CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		response.errorMessage = curl_easy_strerror(res);
		return response;
	}
	long code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
	response.statusCode = static_cast<unsigned long>(code);
	response.body = std::move(responseBody);
	response.success = (response.statusCode == 200);
	return response;
}

NetworkResponse NetworkClient::post(const std::string &url,
									const std::string &body) {
	NetworkResponse response = {false, "", 0, ""};
	if (!m_handle) {
		response.errorMessage = "curl uninitialised";
		return response;
	}
	CURL *curl = static_cast<CURL *>(m_handle);
	std::string responseBody;
	curl_easy_reset(curl);
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
					 static_cast<long>(body.size()));
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "GLStation");
	CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		response.errorMessage = curl_easy_strerror(res);
		return response;
	}
	long code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
	response.statusCode = static_cast<unsigned long>(code);
	response.body = std::move(responseBody);
	response.success = (response.statusCode == 200);
	return response;
}

} // namespace GLStation::Util
#endif

#include "Http.h"

namespace TOOL {

	size_t HttpStringSink(void* contents, size_t size, size_t nmemb, void* userp) {
		const size_t total = size * nmemb;
		std::string* response = static_cast<std::string*>(userp);
		if (response != nullptr) {
			response->append(static_cast<char*>(contents), total);
		}
		return total;
	}

	size_t HttpFileSink(void* contents, size_t size, size_t nmemb, void* userp) {
		FILE* stream = static_cast<FILE*>(userp);
		if (stream == nullptr) {
			return 0;
		}
		return fwrite(contents, size, nmemb, stream);
	}

	void CurlDeleter::operator()(CURL* handle) const {
		if (handle != nullptr) {
			curl_easy_cleanup(handle);
		}
	}

	CurlPtr MakeCurl() {
		return CurlPtr(curl_easy_init());
	}

	namespace {

		struct SlistDeleter { void operator()(curl_slist* list) const { if (list != nullptr) { curl_slist_free_all(list); } } };
		using SlistPtr = std::unique_ptr<curl_slist, SlistDeleter>;

		SlistPtr MakeHeaderList(const std::vector<std::string>& Headers) {
			curl_slist* list = nullptr;
			for (const std::string& header : Headers) {
				list = curl_slist_append(list, header.c_str());
			}
			return SlistPtr(list);
		}

	}

	HttpResult HttpGet(const std::string& url, const std::vector<std::string>& Headers) {
		HttpResult result;
		CurlPtr curl = MakeCurl();
		if (!curl) {
			return result;
		}
		SlistPtr headers = MakeHeaderList(Headers);

		curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
		if (headers) {
			curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());
		}
		curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, HttpStringSink);
		curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &result.Body);

		result.Curl = curl_easy_perform(curl.get());
		curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &result.Code);
		return result;
	}

}
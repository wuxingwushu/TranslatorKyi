#pragma once
//libcurl 的薄封装：共享的写回调 + CURL* 的 RAII 持有 + 一个最简单的 GET。
//原先全项目有 5 份几乎一样的写回调、11 处手写的
//curl_easy_init → setopt → perform → curl_easy_cleanup 样板，这里收敛公共部分；
//WebDAV 那种需要自定义方法/鉴权/上传的请求仍自行 setopt，但用 CurlPtr 托管生命周期。
#include <string>
#include <vector>
#include <memory>
#include <curl/curl.h>

namespace TOOL {

	//接收响应体的共享回调（CURLOPT_WRITEFUNCTION）：userp 传 std::string*
	size_t HttpStringSink(void* contents, size_t size, size_t nmemb, void* userp);

	//接收响应体的共享回调（CURLOPT_WRITEFUNCTION）：userp 传 FILE*
	size_t HttpFileSink(void* contents, size_t size, size_t nmemb, void* userp);

	//RAII 持有 CURL*：离开作用域自动 curl_easy_cleanup
	struct CurlDeleter { void operator()(CURL* handle) const; };
	using CurlPtr = std::unique_ptr<CURL, CurlDeleter>;

	//curl_easy_init 的封装（失败返回空指针）；析构由 CurlPtr 负责
	CurlPtr MakeCurl();

	struct HttpResult {
		CURLcode Curl = CURLE_OK;//perform 的返回值
		long Code = 0;           //HTTP 状态码
		std::string Body;        //响应体
	};

	//GET 一个 URL，Headers 形如 {"User-Agent: ..."}；响应体写进返回值
	HttpResult HttpGet(const std::string& url, const std::vector<std::string>& Headers = {});

}
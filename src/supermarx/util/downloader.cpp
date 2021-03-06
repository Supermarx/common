#include "downloader.hpp"

#include <thread>
#include <memory>
#include <curl/curl.h>

#include <supermarx/util/guard.hpp>

namespace supermarx
{

downloader::error::error(const std::string &_arg)
: std::runtime_error(_arg)
{}

static size_t downloader_write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;

	std::string *mem = static_cast<std::string*>(userp);
	mem->append(static_cast<char*>(contents), realsize);

	return realsize;
}

downloader::curl_ptr downloader::create_handle() const
{
	static bool initialized = false;

	if(!initialized)
	{
		curl_global_init(CURL_GLOBAL_ALL);
		initialized = true;
	}

	CURL* handle = curl_easy_init();

	curl_easy_setopt(handle, CURLOPT_REFERER, referer.c_str());
	curl_easy_setopt(handle, CURLOPT_COOKIE, cookies.c_str());
	curl_easy_setopt(handle, CURLOPT_USERAGENT, agent.c_str());
	curl_easy_setopt(handle, CURLOPT_ENCODING, "UTF-8");
	curl_easy_setopt(handle, CURLOPT_TIMEOUT, 60);
	curl_easy_setopt(handle, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, error_msg.get());
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, downloader_write_callback);

	return curl_ptr(handle, &curl_easy_cleanup);
}

downloader::downloader(const std::string& _agent, unsigned int _ratelimit)
	: agent(_agent)
	, referer()
	, cookies()
	, ratelimit(_ratelimit)
	, last_request()
	, error_msg(new char[CURL_ERROR_SIZE])
{}

void downloader::await_ratelimit()
{
	if(last_request)
	{
		auto todo = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::milliseconds(ratelimit) - last_request->diff());
		if(todo > std::chrono::milliseconds(0))
			std::this_thread::sleep_for(todo);
	}
}

downloader::response downloader::fetch(const std::string& url)
{
	await_ratelimit();
	auto g(make_guard([&](){ last_request = timer(); }));

	std::string result;
	curl_ptr handle(create_handle());

	curl_easy_setopt(handle.get(), CURLOPT_URL, url.c_str());
	curl_easy_setopt(handle.get(), CURLOPT_WRITEDATA, static_cast<void*>(&result));

	if(CURLE_OK != curl_easy_perform(handle.get()))
		throw downloader::error(error_msg.get());

	long http_code = 0;
	curl_easy_getinfo(handle.get(), CURLINFO_RESPONSE_CODE, &http_code);

	return {http_code, result};
}

downloader::response downloader::post(const std::string& url, const downloader::formmap& form)
{
	await_ratelimit();
	auto g(make_guard([&](){ last_request = timer(); }));

	std::string result, formdata;
	curl_ptr handle(create_handle());

	{
		bool first_arg = true;
		for(const auto& kvp : form)
		{
			if(!first_arg)
				formdata.append("&");
			else
				first_arg = false;

			formdata.append(kvp.first);
			formdata.append("=");

			{
				char *arg = curl_easy_escape(handle.get(), kvp.second.c_str(), kvp.second.size());
				formdata.append(arg);
				curl_free(arg);
			}
		}
	}

	curl_slist *headerlist = nullptr;
	curl_slist_append(headerlist, "Expect:");
	curl_easy_setopt(handle.get(), CURLOPT_HTTPHEADER, headerlist);

	curl_easy_setopt(handle.get(), CURLOPT_URL, url.c_str());
	curl_easy_setopt(handle.get(), CURLOPT_POSTFIELDS, formdata.c_str());
	curl_easy_setopt(handle.get(), CURLOPT_WRITEDATA, static_cast<void*>(&result));

	if(CURLE_OK != curl_easy_perform(handle.get()))
		throw downloader::error(error_msg.get());

	long http_code = 0;
	curl_easy_getinfo(handle.get(), CURLINFO_RESPONSE_CODE, &http_code);

	curl_slist_free_all(headerlist);

	return {http_code, result};
}

void downloader::set_referer(const std::string& r)
{
	referer = r;
}

void downloader::set_cookies(const std::string& c)
{
	cookies = c;
}

std::string downloader::escape(const std::string& str) const
{
	curl_ptr handle(create_handle());

	char* buffer =  curl_easy_escape(handle.get(), str.c_str(), str.length());
	std::string result(buffer);
	curl_free(buffer);

	return result;
}

}

#pragma once

#include <memory>
#include <string>
#include <map>

#include <boost/optional.hpp>
#include <supermarx/util/timer.hpp>

typedef void CURL;

namespace supermarx
{

class downloader
{
public:
	typedef std::map<std::string, std::string> formmap;
	typedef std::unique_ptr<CURL, std::function<void(CURL*)>> curl_ptr;

	class error : public std::runtime_error
	{
	public:
		error(std::string const &_arg);
	};

private:
	std::string agent, referer, cookies;

	unsigned int ratelimit;
	boost::optional<timer> last_request;
	std::unique_ptr<char[]> error_msg;

	curl_ptr create_handle() const;
	void await_ratelimit();

public:
	downloader(const std::string& agent, unsigned int ratelimit = 0);

	std::string fetch(const std::string& url);
	std::string post(const std::string& url, const formmap& form);

	void set_cookies(const std::string& cookies);
	void set_referer(const std::string& referer);

	downloader(downloader&) = delete;
	void operator=(downloader&) = delete;
};

}

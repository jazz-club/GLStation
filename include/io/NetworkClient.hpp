#pragma once

#include "util/Types.hpp"
#include <string>

namespace GLStation::Util {

struct NetworkResponse {
	bool success;
	std::string body;
	unsigned long statusCode;
	std::string errorMessage;
};

class NetworkClient {
  public:
	NetworkClient();
	~NetworkClient();
	NetworkResponse get(const std::string &url);
	NetworkResponse post(const std::string &url, const std::string &body);

  private:
	void *m_handle;
};

} // namespace GLStation::Util

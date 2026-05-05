#pragma once

#include <string>

namespace GLStation::UI {

struct Theme {
	static const std::string &cyan();
	static const std::string &yellow();
	static const std::string &green();
	static const std::string &red();
	static const std::string &bold();
	static const std::string &dim();
	static const std::string &reset();
};

} // namespace GLStation::UI

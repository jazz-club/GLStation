#pragma once
#include <string>

namespace GLStation::Log {

struct Logger {
	static void info(const std::string &msg);
	static void warn(const std::string &msg);
	static void error(const std::string &msg);
};

} // namespace GLStation::Log

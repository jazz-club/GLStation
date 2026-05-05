#include "log/Logger.hpp"
#include "ui/Theme.hpp"
#include <iostream>

namespace GLStation::Log {

void Logger::info(const std::string &msg) {
	std::cout << UI::Theme::cyan() << "[INFO] " << UI::Theme::reset() << msg
			  << std::endl;
}

void Logger::warn(const std::string &msg) {
	std::cout << UI::Theme::yellow() << "[WARN] " << UI::Theme::reset() << msg
			  << std::endl;
}

void Logger::error(const std::string &msg) {
	std::cerr << UI::Theme::red() << "[ERROR] " << UI::Theme::reset() << msg
			  << std::endl;
}

} // namespace GLStation::Log

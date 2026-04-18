#include "log/Logger.hpp"
#include "ui/Terminal.hpp"
#include <iostream>

namespace GLStation::Log {

void Logger::info(const std::string &msg) {
	std::string c = UI::isAnsiEnabled() ? UI::ANSI_CYAN : "";
	std::string r = UI::isAnsiEnabled() ? UI::ANSI_RESET : "";
	std::cout << c << "[INFO] " << r << msg << std::endl;
}

void Logger::warn(const std::string &msg) {
	std::string y = UI::isAnsiEnabled() ? UI::ANSI_YELLOW : "";
	std::string r = UI::isAnsiEnabled() ? UI::ANSI_RESET : "";
	std::cout << y << "[WARN] " << r << msg << std::endl;
}

void Logger::error(const std::string &msg) {
	std::string red = UI::isAnsiEnabled() ? UI::ANSI_RED : "";
	std::string r = UI::isAnsiEnabled() ? UI::ANSI_RESET : "";
	std::cerr << red << "[ERROR] " << r << msg << std::endl;
}

} // namespace GLStation::Log

#include "ui/Terminal.hpp"
#include "ui/Theme.hpp"

namespace GLStation::UI {

static const std::string kEmpty;
static const std::string kCyan = "\033[36m";
static const std::string kYellow = "\033[33m";
static const std::string kGreen = "\033[32m";
static const std::string kRed = "\033[31m";
static const std::string kBold = "\033[1m";
static const std::string kDim = "\033[90m";
static const std::string kReset = "\033[0m";

const std::string &Theme::cyan() { return isAnsiEnabled() ? kCyan : kEmpty; }
const std::string &Theme::yellow() {
	return isAnsiEnabled() ? kYellow : kEmpty;
}
const std::string &Theme::green() { return isAnsiEnabled() ? kGreen : kEmpty; }
const std::string &Theme::red() { return isAnsiEnabled() ? kRed : kEmpty; }
const std::string &Theme::bold() { return isAnsiEnabled() ? kBold : kEmpty; }
const std::string &Theme::dim() { return isAnsiEnabled() ? kDim : kEmpty; }
const std::string &Theme::reset() { return isAnsiEnabled() ? kReset : kEmpty; }

std::string Theme::strip(const std::string &s) {
	std::string result;
	result.reserve(s.size());
	for (size_t i = 0; i < s.size(); ++i) {
		if (s[i] == '\033' && i + 1 < s.size() && s[i + 1] == '[') {
			size_t j = i + 2;
			while (j < s.size() && (s[j] < 0x40 || s[j] > 0x7E))
				++j;
			i = j;
		} else {
			result += s[i];
		}
	}
	return result;
}

} // namespace GLStation::UI

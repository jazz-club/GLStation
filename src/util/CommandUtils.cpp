#include "util/CommandUtils.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

namespace GLStation::IO::Commands {

GLStation::Core::u64
CommandUtils::parseTicksFromString(const std::string &arg) {
	if (arg.empty())
		return 1;
	try {
		if (arg.back() == 's' || arg.back() == 'S')
			return static_cast<GLStation::Core::u64>(
					   std::stoull(arg.substr(0, arg.size() - 1))) *
				   1000;
		if (arg.back() == 'm' || arg.back() == 'M')
			return static_cast<GLStation::Core::u64>(
					   std::stoull(arg.substr(0, arg.size() - 1))) *
				   60000;
		return static_cast<GLStation::Core::u64>(std::stoull(arg));
	} catch (...) {
	}
	return 1;
}

/*
	this is intuitive  for me but theres other ways that might make more sense for some people
	please chime in if theres a smarter way to parse time
*/
bool CommandUtils::parseRunDurationTicks(const std::string &arg,
										 GLStation::Core::u64 &outTicks) {
	if (arg.empty())
		return false;
	std::string s = arg;
	for (char &c : s) {
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}

	long double factorMs = 1000.0L;
	size_t suffixLen = 0;
	if (s.size() >= 2 && s.substr(s.size() - 2) == "ms") {
		factorMs = 1.0L;
		suffixLen = 2;
	} else if (!s.empty() && s.back() == 's') {
		factorMs = 1000.0L;
		suffixLen = 1;
	} else if (!s.empty() && s.back() == 'm') {
		factorMs = 60000.0L;
		suffixLen = 1;
	} else if (!s.empty() && s.back() == 'h') {
		factorMs = 3600000.0L;
		suffixLen = 1;
	}

	std::string numPart =
		(suffixLen > 0) ? s.substr(0, s.size() - suffixLen) : s;
	if (numPart.empty())
		return false;

	try {
		long double value = std::stold(numPart);
		if (!(value >= 0))
			return false;
		long double ms = value * factorMs;
		if (ms < 1.0L)
			ms = 1.0L;

		outTicks = static_cast<GLStation::Core::u64>(
			std::llround(std::min<long double>(
				ms, static_cast<long double>(
						std::numeric_limits<GLStation::Core::u64>::max()))));
		return outTicks > 0;
	} catch (...) {
	}
	return false;
}

} // namespace GLStation::IO::Commands

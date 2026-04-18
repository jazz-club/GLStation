#pragma once
#include "sim/Engine.hpp"
#include <string>

namespace GLStation::IO::Commands {

struct CommandUtils {
	static GLStation::Core::u64 parseTicksFromString(const std::string &arg);
	static bool parseRunDurationTicks(const std::string &arg,
									  GLStation::Core::u64 &outTicks);
};

} // namespace GLStation::IO::Commands

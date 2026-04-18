#pragma once
#include "sim/Engine.hpp"
#include <string>

namespace GLStation::IO::Commands {
struct Run {
	static void execute(Simulation::Engine &engine, const std::string &arg,
						const std::string &extra);
};
} // namespace GLStation::IO::Commands

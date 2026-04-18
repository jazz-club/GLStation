#pragma once
#include "sim/Engine.hpp"
#include <string>

namespace GLStation::IO::Commands {
struct Import {
	static void execute(Simulation::Engine &engine,
						const std::string &cityName);
};
} // namespace GLStation::IO::Commands

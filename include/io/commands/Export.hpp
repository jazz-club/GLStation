#pragma once
#include "sim/Engine.hpp"
#include <string>

namespace GLStation::IO::Commands {
struct Export {
	static void execute(Simulation::Engine &engine,
						const std::string &filename);
};
} // namespace GLStation::IO::Commands

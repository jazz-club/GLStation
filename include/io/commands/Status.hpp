#pragma once
#include "sim/Engine.hpp"

namespace GLStation::IO::Commands {
struct Status {
	static void execute(Simulation::Engine &engine);
};
} // namespace GLStation::IO::Commands

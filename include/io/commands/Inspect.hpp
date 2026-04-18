#pragma once
#include "sim/Engine.hpp"

namespace GLStation::IO::Commands {
struct Inspect {
	static void execute(Simulation::Engine &engine, GLStation::Core::u64 id);
};
} // namespace GLStation::IO::Commands

#pragma once
#include "sim/Engine.hpp"
#include <string>

namespace GLStation::IO::Commands {
struct Breaker {
	static void execute(Simulation::Engine &engine, const std::string &cmdNorm,
						GLStation::Core::u64 id);
};
} // namespace GLStation::IO::Commands

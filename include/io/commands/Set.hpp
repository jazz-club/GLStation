#pragma once
#include "sim/Engine.hpp"
#include <string>

namespace GLStation::IO::Commands {
struct Set {
	static void execute(Simulation::Engine &engine, GLStation::Core::u64 id,
						const std::string &param, double val);
};
} // namespace GLStation::IO::Commands

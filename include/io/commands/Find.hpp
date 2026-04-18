#pragma once
#include "sim/Engine.hpp"
#include <string>

namespace GLStation::IO::Commands {
struct Find {
	static void execute(Simulation::Engine &engine, const std::string &query);
};
} // namespace GLStation::IO::Commands

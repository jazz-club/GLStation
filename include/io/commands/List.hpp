#pragma once
#include "sim/Engine.hpp"
#include <string>

namespace GLStation::IO::Commands {
struct List {
	static void execute(Simulation::Engine &engine, const std::string &type);
};
} // namespace GLStation::IO::Commands

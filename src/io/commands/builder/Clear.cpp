#include "grid/builder/Builder.hpp"
#include "io/commands/builder/Clear.hpp"
#include "sim/Engine.hpp"
#include <iostream>

namespace GLStation::IO::Commands::Builder {

void Clear::execute(Simulation::Engine &engine, std::stringstream &ss) {
	(void)ss;
	engine.clearSubstations();
	Grid::Builder::Builder::setActiveSubstation(nullptr);
	std::cout << "Cleared all substations.\n";
}

} // namespace GLStation::IO::Commands::Builder

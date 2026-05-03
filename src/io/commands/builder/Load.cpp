#include "grid/builder/Builder.hpp"
#include "io/commands/builder/Load.hpp"
#include "sim/Engine.hpp"
#include <iostream>

namespace GLStation::IO::Commands::Builder {

void Load::execute(Simulation::Engine &engine, std::stringstream &ss) {
	std::string file;
	ss >> file;
	if (!file.empty()) {
		engine.loadGrid(file);
		if (!engine.getSubstations().empty())
			Grid::Builder::Builder::setActiveSubstation(
				engine.getSubstations().front());
	} else {
		std::cout << "Usage: load <filename.csv>\n";
	}
}

} // namespace GLStation::IO::Commands::Builder

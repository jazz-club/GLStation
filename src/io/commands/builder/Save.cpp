#include "io/commands/builder/Save.hpp"
#include "sim/Engine.hpp"
#include <iostream>

namespace GLStation::IO::Commands::Builder {

void Save::execute(Simulation::Engine &engine, std::stringstream &ss) {
	std::string file;
	ss >> file;
	if (!file.empty()) {
		engine.saveGrid(file);
	} else {
		std::cout << "Usage: save <filename.csv>\n";
	}
}

} // namespace GLStation::IO::Commands::Builder

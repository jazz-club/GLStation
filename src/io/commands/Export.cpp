#include "io/commands/Export.hpp"
#include <iostream>

namespace GLStation::IO::Commands {

void Export::execute(Simulation::Engine &engine, const std::string &filename) {
	engine.exportVoltagesToCSV(filename);
	std::cout << "Exported to " << filename << std::endl;
}

} // namespace GLStation::IO::Commands

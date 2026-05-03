#include "grid/Substation.hpp"
#include "grid/builder/Builder.hpp"
#include "io/commands/builder/Use.hpp"
#include "sim/Engine.hpp"
#include <iostream>

namespace GLStation::IO::Commands::Builder {

void Use::execute(Simulation::Engine &engine, std::stringstream &ss) {
	std::string name;
	ss >> name;
	if (name.empty()) {
		std::cout << "Usage: use <substation_name>\n";
		return;
	}
	bool found = false;
	for (auto &sub : engine.getSubstations()) {
		if (sub->getName() == name) {
			Grid::Builder::Builder::setActiveSubstation(sub);
			found = true;
			break;
		}
	}
	if (!found)
		std::cout << "Substation '" << name << "' not found.\n";
}

} // namespace GLStation::IO::Commands::Builder

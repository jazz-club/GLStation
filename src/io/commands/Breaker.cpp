#include "grid/Breaker.hpp"
#include "grid/Generator.hpp"
#include "grid/Line.hpp"
#include "grid/Load.hpp"
#include "grid/Node.hpp"
#include "grid/Transformer.hpp"
#include "io/commands/Breaker.hpp"
#include <iostream>

namespace GLStation::IO::Commands {

void Breaker::execute(Simulation::Engine &engine, const std::string &cmdNorm,
					  GLStation::Core::u64 id) {
	bool found = false;
	for (const auto &sub : engine.getSubstations()) {
		for (const auto &comp : sub->getComponents()) {
			if (comp->getId() == id) {
				if (auto breaker = dynamic_cast<Grid::Breaker *>(comp.get())) {
					breaker->setOpen(cmdNorm == "open");
					std::cout << "Breaker #" << id << " (" << comp->getName()
							  << ") is now "
							  << (breaker->isOpen() ? "OPEN" : "CLOSED")
							  << std::endl;
				} else
					std::cout << "Error: ID " << id << " is not a breaker."
							  << std::endl;
				found = true;
				break;
			}
		}
	}
	if (!found)
		std::cout << "No component with ID " << id << std::endl;
}

} // namespace GLStation::IO::Commands

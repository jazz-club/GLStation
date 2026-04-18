#include "grid/Breaker.hpp"
#include "grid/Generator.hpp"
#include "grid/Line.hpp"
#include "grid/Load.hpp"
#include "grid/Node.hpp"
#include "grid/Transformer.hpp"
#include "io/commands/Inspect.hpp"
#include "ui/Terminal.hpp"
#include <iostream>

namespace GLStation::IO::Commands {

void Inspect::execute(Simulation::Engine &engine, GLStation::Core::u64 id) {
	bool found = false;
	for (const auto &sub : engine.getSubstations()) {
		for (const auto &comp : sub->getComponents()) {
			if (comp->getId() == id) {
				std::string cyn = UI::isAnsiEnabled() ? UI::ANSI_CYAN : "";
				std::string res = UI::isAnsiEnabled() ? UI::ANSI_RESET : "";
				std::cout << "\n"
						  << cyn << "--- Component Inspect: " << comp->getName()
						  << " ---" << res << std::endl;
				std::cout << "Type:     " << typeid(*comp).name() << std::endl;
				std::cout << "Summary:  " << comp->toString() << std::endl;

				if (auto line = dynamic_cast<Grid::Line *>(comp.get())) {
					std::cout << "Details:  Limit=" << line->getCurrentLimit()
							  << "A" << std::endl;
				} else if (auto gen =
							   dynamic_cast<Grid::Generator *>(comp.get())) {
					std::cout << "Details:  P_target=" << gen->getTargetP()
							  << "kW, V_target=" << gen->getTargetV() << "pu"
							  << std::endl;
				} else if (auto load = dynamic_cast<Grid::Load *>(comp.get())) {
					std::cout << "Details:  P_max=" << load->getMaxPower()
							  << "kW, PF=" << load->getPowerFactor()
							  << std::endl;
				}
				found = true;
				break;
			}
		}
	}
	if (!found)
		std::cout << "No component with matching ID" << std::endl;
}

} // namespace GLStation::IO::Commands

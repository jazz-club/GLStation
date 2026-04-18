#include "grid/Breaker.hpp"
#include "grid/Generator.hpp"
#include "grid/Line.hpp"
#include "grid/Load.hpp"
#include "grid/Node.hpp"
#include "grid/Transformer.hpp"
#include "io/commands/Set.hpp"
#include "io/handlers/InputHandler.hpp"
#include <iostream>

namespace GLStation::IO::Commands {

void Set::execute(Simulation::Engine &engine, GLStation::Core::u64 id,
				  const std::string &paramArg, double val) {
	std::string param = Util::InputHandler::normaliseForComparison(paramArg);
	bool ok = false;
	for (const auto &sub : engine.getSubstations()) {
		for (const auto &comp : sub->getComponents()) {
			if (comp->getId() == id) {
				if (auto gen = dynamic_cast<Grid::Generator *>(comp.get())) {
					if (param == "p") {
						gen->setTargetP(val);
						ok = true;
					} else if (param == "v") {
						gen->setTargetV(val);
						ok = true;
					}
				} else if (auto load = dynamic_cast<Grid::Load *>(comp.get())) {
					if (param == "pf") {
						load->setPowerFactor(val);
						ok = true;
					} else if (param == "p") {
						load->setMaxPower(val);
						ok = true;
					}
				} else if (auto line = dynamic_cast<Grid::Line *>(comp.get())) {
					if (param == "limit") {
						line->setCurrentLimit(val);
						ok = true;
					}
				} else if (auto trafo =
							   dynamic_cast<Grid::Transformer *>(comp.get())) {
					if (param == "limit") {
						trafo->setCurrentLimit(val);
						ok = true;
					} else if (param == "tap") {
						trafo->setTap(val);
						ok = true;
					}
				}
				if (ok)
					std::cout << "Param" << comp->getName() << " " << param
							  << " set to " << val << std::endl;
				break;
			}
		}
	}
	if (!ok) {
		bool hasId = false;
		for (const auto &sub : engine.getSubstations())
			for (const auto &comp : sub->getComponents())
				if (comp->getId() == id) {
					hasId = true;
					break;
				}
		if (!hasId)
			std::cout << "Error: ID " << id << std::endl;
		else
			std::cout << "Parameter '" << param
					  << "' not allowed for this component."
						 "(gen: p,v; load: p,pf; line/trafo: "
						 "limit; trafo: tap)"
					  << std::endl;
	}
}

} // namespace GLStation::IO::Commands

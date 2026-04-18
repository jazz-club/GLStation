#include "grid/Breaker.hpp"
#include "grid/Generator.hpp"
#include "grid/Line.hpp"
#include "grid/Load.hpp"
#include "grid/Node.hpp"
#include "grid/Transformer.hpp"
#include "io/commands/List.hpp"
#include "io/handlers/InputHandler.hpp"
#include "ui/Terminal.hpp"
#include <iostream>

namespace GLStation::IO::Commands {

void List::execute(Simulation::Engine &engine, const std::string &typeArg) {
	std::string type = Util::InputHandler::normaliseForComparison(typeArg);
	if (type.empty()) {
		std::cout << "Error: Missing type. Usage: list "
					 "<gen|load|line|trafo|breaker>"
				  << std::endl;
	} else {
		std::string cyn = UI::isAnsiEnabled() ? UI::ANSI_CYAN : "";
		std::string res = UI::isAnsiEnabled() ? UI::ANSI_RESET : "";
		std::cout << "\n"
				  << cyn << "--- List [" << type << "] ---" << res << std::endl;
		bool validType = (type == "gen" || type == "load" || type == "line" ||
						  type == "trafo" || type == "breaker");
		if (!validType) {
			std::cout << "  Error: Unknown type \"" << type
					  << "\". Use: gen, load, line, trafo, breaker."
					  << std::endl;
		} else {
			int n = 0;
			for (const auto &sub : engine.getSubstations()) {
				for (const auto &comp : sub->getComponents()) {
					bool match = false;
					if (type == "gen" &&
						dynamic_cast<Grid::Generator *>(comp.get()))
						match = true;
					if (type == "load" &&
						dynamic_cast<Grid::Load *>(comp.get()))
						match = true;
					if (type == "line" &&
						dynamic_cast<Grid::Line *>(comp.get()))
						match = true;
					if (type == "trafo" &&
						dynamic_cast<Grid::Transformer *>(comp.get()))
						match = true;
					if (type == "breaker" &&
						dynamic_cast<Grid::Breaker *>(comp.get()))
						match = true;
					if (match) {
						std::cout << "  - [ID: " << comp->getId() << "] "
								  << comp->getName() << std::endl;
						++n;
					}
				}
			}
			if (n == 0)
				std::cout << "  (none)" << std::endl;
		}
	}
}

} // namespace GLStation::IO::Commands

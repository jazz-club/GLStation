#include "grid/Breaker.hpp"
#include "grid/Generator.hpp"
#include "grid/Line.hpp"
#include "grid/Load.hpp"
#include "grid/Node.hpp"
#include "grid/Transformer.hpp"
#include "io/commands/Commands.hpp"
#include "io/handlers/InputHandler.hpp"
#include "log/Logger.hpp"
#include "sim/Engine.hpp"
#include "ui/Theme.hpp"
#include <iostream>

namespace GLStation::IO::Commands {

void cmdList(Simulation::Engine &engine, const std::vector<std::string> &args) {
	if (args.empty()) {
		Log::Logger::warn("Usage: list <gen|load|line|trafo|breaker>");
		return;
	}
	std::string type = InputHandler::normaliseForComparison(args[0]);
	std::cout << "\n"
			  << UI::Theme::cyan() << "--- List [" << type << "] ---"
			  << UI::Theme::reset() << std::endl;
	bool validType = (type == "gen" || type == "load" || type == "line" ||
					  type == "trafo" || type == "breaker");
	if (!validType) {
		Log::Logger::error("Unknown type \"" + type +
						   "\". Use: gen, load, line, trafo, breaker.");
		return;
	}
	int n = 0;
	for (const auto &sub : engine.getSubstations()) {
		for (const auto &comp : sub->getComponents()) {
			bool match = false;
			if (type == "gen" && dynamic_cast<Grid::Generator *>(comp.get()))
				match = true;
			if (type == "load" && dynamic_cast<Grid::Load *>(comp.get()))
				match = true;
			if (type == "line" && dynamic_cast<Grid::Line *>(comp.get()))
				match = true;
			if (type == "trafo" &&
				dynamic_cast<Grid::Transformer *>(comp.get()))
				match = true;
			if (type == "breaker" && dynamic_cast<Grid::Breaker *>(comp.get()))
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

} // namespace GLStation::IO::Commands

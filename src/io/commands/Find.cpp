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
#include <iomanip>
#include <iostream>
#include <utility>
#include <vector>

namespace GLStation::IO::Commands {

void cmdFind(Simulation::Engine &engine, const std::vector<std::string> &args) {
	if (args.empty()) {
		Log::Logger::warn("Usage: find <query>");
		return;
	}
	std::string q = InputHandler::trim(args[0]);
	std::cout << "\n"
			  << UI::Theme::cyan() << "--- Search: '" << q << "' ---"
			  << UI::Theme::reset() << std::endl;
	bool found = false;
	std::vector<std::pair<Core::u64, std::string>> partial;
	std::string qNorm = InputHandler::normaliseForComparison(q);
	for (const auto &sub : engine.getSubstations()) {
		for (const auto &comp : sub->getComponents()) {
			std::string name = comp->getName();
			std::string nameNorm = InputHandler::normaliseForComparison(name);
			if (name.find(q) != std::string::npos ||
				(!q.empty() && nameNorm.find(qNorm) != std::string::npos)) {
				std::cout << "  - [ID: " << std::setw(2) << comp->getId()
						  << "] " << std::setw(15) << name << " ("
						  << typeid(*comp).name() << ")" << std::endl;
				found = true;
			} else if (qNorm.size() >= 2 &&
					   nameNorm.find(qNorm) != std::string::npos)
				partial.push_back({comp->getId(), name});
		}
	}
	if (!found) {
		if (!partial.empty()) {
			std::cout << "  No exact match for \"" << q
					  << "\". Partial matches:" << std::endl;
			for (const auto &p : partial)
				std::cout << "  - [ID: " << std::setw(2) << p.first << "] "
						  << p.second << std::endl;
		} else {
			std::cout << "  No matches found." << std::endl;
		}
	}
}

} // namespace GLStation::IO::Commands

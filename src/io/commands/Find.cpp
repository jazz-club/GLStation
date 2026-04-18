#include "io/commands/Find.hpp"
#include "io/handlers/InputHandler.hpp"
#include "ui/Terminal.hpp"
#include <iomanip>
#include <iostream>
#include <utility>
#include <vector>

namespace GLStation::IO::Commands {

void Find::execute(Simulation::Engine &engine, const std::string &query) {
	std::string q = Util::InputHandler::trim(query);
	std::string cyn = UI::isAnsiEnabled() ? UI::ANSI_CYAN : "";
	std::string res = UI::isAnsiEnabled() ? UI::ANSI_RESET : "";
	std::cout << "\n"
			  << cyn << "--- Search: '" << q << "' ---" << res << std::endl;
	bool found = false;
	std::vector<std::pair<Core::u64, std::string>> partial;
	std::string qNorm = Util::InputHandler::normaliseForComparison(q);
	for (const auto &sub : engine.getSubstations()) {
		for (const auto &comp : sub->getComponents()) {
			std::string name = comp->getName();
			std::string nameNorm =
				Util::InputHandler::normaliseForComparison(name);
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

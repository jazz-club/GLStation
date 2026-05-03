#include "grid/Breaker.hpp"
#include "grid/Generator.hpp"
#include "grid/Line.hpp"
#include "grid/Load.hpp"
#include "grid/Node.hpp"
#include "grid/Shunt.hpp"
#include "grid/Substation.hpp"
#include "grid/Transformer.hpp"
#include "grid/builder/Builder.hpp"
#include "io/commands/Inspect.hpp"
#include "io/commands/Tree.hpp"
#include "io/commands/builder/Add.hpp"
#include "io/commands/builder/Clear.hpp"
#include "io/commands/builder/Generate.hpp"
#include "io/commands/builder/Load.hpp"
#include "io/commands/builder/Save.hpp"
#include "io/commands/builder/Use.hpp"
#include "io/commands/builder/Validate.hpp"
#include "io/handlers/InputHandler.hpp"
#include "sim/Engine.hpp"
#include "ui/Terminal.hpp"
#include "util/Random.hpp"
#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <vector>

namespace GLStation::Grid::Builder {

static std::shared_ptr<Grid::Substation> activeSubstation = nullptr;

std::shared_ptr<Grid::Substation> Builder::getActiveSubstation() {
	return activeSubstation;
}
void Builder::setActiveSubstation(std::shared_ptr<Grid::Substation> sub) {
	activeSubstation = sub;
}

void Builder::runLoop(Simulation::Engine &engine) {
	std::string line;
	std::string cyn = UI::isAnsiEnabled() ? UI::ANSI_CYAN : "";
	std::string res = UI::isAnsiEnabled() ? UI::ANSI_RESET : "";
	std::string yel = UI::isAnsiEnabled() ? UI::ANSI_YELLOW : "";

	std::cout << cyn << "\nEntering Grid Builder Mode." << res << " Type "
			  << yel << "'help'" << res << " for commands, " << yel << "'exit'"
			  << res << " to return.\n"
			  << std::endl;

	if (engine.getSubstations().empty()) {
		auto defaultSub =
			std::make_shared<Grid::Substation>("Default_Substation");
		engine.addSubstation(defaultSub);
		activeSubstation = defaultSub;
	} else {
		activeSubstation = engine.getSubstations().front();
	}

	while (true) {
		std::string subName =
			activeSubstation ? activeSubstation->getName() : "None";
		std::cout << cyn << "builder" << res << "[" << subName << "]> "
				  << std::flush;
		line = Util::InputHandler::readLineWithHistory();
		if (line == "exit" && !std::cin.good())
			break;
		line = Util::InputHandler::trim(line);
		if (line.empty())
			continue;

		std::stringstream ss(line);
		std::string cmd;
		ss >> cmd;
		std::string cmdNorm = Util::InputHandler::normaliseForComparison(cmd);

		if (cmdNorm == "exit") {
			break;
		} else if (cmdNorm == "help") {
			std::cout
				<< cyn << "Builder Commands:\n"
				<< res << "  " << yel << "add substation" << res << " <name>\n"
				<< "  " << yel << "add node" << res << " <name> <baseVoltage>\n"
				<< "  " << yel << "add line" << res
				<< " <name> <from> <to> <r> <x> [b]\n"
				<< "  " << yel << "add load" << res
				<< " <name> <node> <max_kw>\n"
				<< "  " << yel << "add generator" << res
				<< " <name> <node> <mode(slack/pv/pq)> <target_p> <target_v>\n"
				<< "  " << yel << "add transformer" << res
				<< " <name> <pri> <sec> <r> <x> <tap>\n"
				<< "  " << yel << "add breaker" << res
				<< " <name> <from> <to>\n"
				<< "  " << yel << "add shunt" << res
				<< " <name> <node> <g> <b>\n"
				<< "  " << yel << "save" << res << " <filename.csv>\n"
				<< "  " << yel << "load" << res << " <filename.csv>\n"
				<< "  " << yel << "generate grid" << res
				<< " <name> <population>\n"
				<< "  " << yel << "validate" << res << "\n"
				<< "  " << yel << "tree" << res << "\n"
				<< "  " << yel << "inspect" << res << " <id>\n"
				<< "  " << yel << "use" << res << " <substation_name>\n"
				<< "  " << yel << "clear" << res << "\n"
				<< "  " << yel << "exit" << res << "\n";
		} else if (cmdNorm == "use") {
			::GLStation::IO::Commands::Builder::Use::execute(engine, ss);
		} else if (cmdNorm == "clear") {
			::GLStation::IO::Commands::Builder::Clear::execute(engine, ss);
		} else if (cmdNorm == "generate") {
			::GLStation::IO::Commands::Builder::Generate::execute(engine, ss);
		} else if (cmdNorm == "save") {
			::GLStation::IO::Commands::Builder::Save::execute(engine, ss);
		} else if (cmdNorm == "load") {
			::GLStation::IO::Commands::Builder::Load::execute(engine, ss);
		} else if (cmdNorm == "validate") {
			::GLStation::IO::Commands::Builder::Validate::execute(engine, ss);
		} else if (cmdNorm == "tree") {
			IO::Commands::Tree::execute(engine);
		} else if (cmdNorm == "inspect") {
			Core::u64 id;
			if (ss >> id)
				IO::Commands::Inspect::execute(engine, id);
			else
				std::cout << "Usage: inspect <id>" << std::endl;
		} else if (cmdNorm == "add") {
			::GLStation::IO::Commands::Builder::Add::execute(engine, ss);
		} else {
			std::cout << "Unknown Builder Command '" << cmdNorm << "'."
					  << std::endl;
		}
	}
}

} // namespace GLStation::Grid::Builder

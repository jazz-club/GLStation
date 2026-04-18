#include "io/commands/Help.hpp"
#include "io/handlers/CommandHandler.hpp"

#include "io/commands/Breaker.hpp"
#include "io/commands/Export.hpp"
#include "io/commands/Find.hpp"
#include "io/commands/Import.hpp"
#include "io/commands/Inspect.hpp"
#include "io/commands/List.hpp"
#include "io/commands/Run.hpp"
#include "io/commands/Set.hpp"
#include "io/commands/Status.hpp"
#include "io/commands/Tree.hpp"
#include "io/handlers/InputHandler.hpp"
#include "sim/Engine.hpp"
#include "ui/Terminal.hpp"
#include <iostream>
#include <sstream>

namespace GLStation::IO {

void runLoop(Simulation::Engine &engine) {
	std::string line;
	while (true) {
		std::string cyn = UI::isAnsiEnabled() ? UI::ANSI_CYAN : "";
		std::string res = UI::isAnsiEnabled() ? UI::ANSI_RESET : "";

		std::cout << cyn << "glstation" << res << "@" << engine.getTickCount()
				  << "ms > " << std::flush;
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
			Commands::Help::execute();

		} else if (cmdNorm == "run") {
			std::string arg;
			std::string extra;
			ss >> arg;
			if (ss >> extra) {
			}
			Commands::Run::execute(engine, arg, extra);
		} else if (cmdNorm == "status") {
			Commands::Status::execute(engine);
		} else if (cmdNorm == "find") {
			std::string query;
			ss >> query;
			Commands::Find::execute(engine, query);
		} else if (cmdNorm == "tree") {
			Commands::Tree::execute(engine);
		} else if (cmdNorm == "list") {
			std::string type;
			ss >> type;
			Commands::List::execute(engine, type);
		} else if (cmdNorm == "inspect") {
			Core::u64 id;
			if (ss >> id)
				Commands::Inspect::execute(engine, id);
			else
				std::cout << "Usage: inspect <id>." << std::endl;
		} else if (cmdNorm == "set") {
			Core::u64 id;
			std::string param;
			double val;
			if (ss >> id >> param >> val)
				Commands::Set::execute(engine, id, param, val);
			else
				std::cout << "Usage: set <id> <param> <value>." << std::endl;
		} else if (cmdNorm == "open" || cmdNorm == "close") {
			Core::u64 id;
			if (ss >> id)
				Commands::Breaker::execute(engine, cmdNorm, id);
			else
				std::cout << "Usage: open <id>  or  close <id>" << std::endl;
		} else if (cmdNorm == "export") {
			std::string filename = "gls.csv";
			ss >> filename;
			Commands::Export::execute(engine, filename);
		} else if (cmdNorm == "import") {
			std::string cityName;
			std::getline(ss, cityName);
			Commands::Import::execute(engine, cityName);
		} else {
			std::cout << "Error: Unknown Command '" << cmd << "'." << std::endl;
		}
	}
}

} // namespace GLStation::IO

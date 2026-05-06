#include "grid/builder/Builder.hpp"
#include "io/CommandDef.hpp"
#include "io/handlers/CommandHandler.hpp"
#include "io/handlers/InputHandler.hpp"
#include "log/Logger.hpp"
#include "sim/Engine.hpp"
#include "ui/Theme.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace GLStation::IO {

enum class ShellMode { Main, Builder };

static void printPrompt(Simulation::Engine &engine, ShellMode mode) {
	if (mode == ShellMode::Main) {
		std::cout << UI::Theme::cyan() << "glstation" << UI::Theme::reset()
				  << "@" << engine.getTickCount() << "ms > " << std::flush;
	} else {
		auto sub = Grid::Builder::BuilderShell::getActiveSubstation();
		std::string subName = sub ? sub->getName() : "None";
		std::cout << UI::Theme::cyan() << "builder" << UI::Theme::reset() << "["
				  << subName << "]> " << std::flush;
	}
}

static std::vector<std::string> tokenise(const std::string &rest) {
	std::vector<std::string> args;
	std::istringstream ss(rest);
	std::string tok;
	while (ss >> tok)
		args.push_back(tok);
	return args;
}

void runLoop(Simulation::Engine &engine) {
	ShellMode mode = ShellMode::Main;
	std::string line;

	for (;;) {
		printPrompt(engine, mode);
		line = InputHandler::readLineWithHistory();
		if (line == "exit" && !std::cin.good())
			break;
		line = InputHandler::trim(line);
		if (line.empty())
			continue;

		std::istringstream ss(line);
		std::string cmd;
		ss >> cmd;
		std::string cmdNorm = InputHandler::normaliseForComparison(cmd);

		if (cmdNorm == "exit") {
			if (mode == ShellMode::Builder) {
				mode = ShellMode::Main;
				Log::Logger::info("Exited builder mode.");
				continue;
			}
			break;
		}

		if (cmdNorm == "help") {
			const auto &table = (mode == ShellMode::Main)
									? getMainCommands()
									: getBuilderCommands();
			printHelp(table);
			if (mode == ShellMode::Main) {
				std::cout << UI::Theme::green() << "  build" << UI::Theme::dim()
						  << "Enter grid builder mode" << UI::Theme::reset()
						  << "\n"
						  << std::endl;
			}
			continue;
		}

		if (mode == ShellMode::Main &&
			(cmdNorm == "build" || cmdNorm == "edit")) {
			mode = ShellMode::Builder;
			std::cout << UI::Theme::cyan() << "Entering builder mode."
					  << UI::Theme::reset()
					  << " Type 'help' for commands, 'exit' to return.\n";
			continue;
		}

		std::string rest;
		std::getline(ss, rest);
		rest = InputHandler::trim(rest);
		std::vector<std::string> args = tokenise(rest);

		const auto &table = (mode == ShellMode::Main) ? getMainCommands()
													  : getBuilderCommands();

		bool found = false;
		for (const auto &def : table) {
			if (def.name == cmdNorm) {
				def.handler(engine, args);
				found = true;

				if (cmdNorm == "set" || cmdNorm == "open" ||
					cmdNorm == "close" || cmdNorm == "add" ||
					cmdNorm == "generate" || cmdNorm == "clear") {
					engine.saveGrid("grid.csv");
				}

				break;
			}
		}

		if (!found)
			Log::Logger::error("Unknown command '" + cmd +
							   "'. Type 'help' for a list.");
	}
}

} // namespace GLStation::IO

#include "grid/builder/Builder.hpp"
#include "io/commands/BuilderCommands.hpp"
#include "io/handlers/InputHandler.hpp"
#include "log/Diagnostics.hpp"
#include "log/Logger.hpp"
#include "sim/Engine.hpp"
#include <filesystem>

namespace GLStation::IO::Commands::Builder {

void cmdLoad(Simulation::Engine &engine, const std::vector<std::string> &args) {
	if (args.empty()) {
		Log::Logger::warn("Usage: load <filename.csv> | load demo");
		return;
	}

	if (!IO::InputHandler::confirmAction("Generative actions require the grid "
										 "and log file to be reset, continue?"))
		return;

	std::filesystem::remove("grid.csv");
	std::filesystem::remove("log.csv");
	Log::Diagnostics::clear();
	engine.getEventManager().clear();

	if (args[0] == "demo") {
		engine.clearSubstations();
		engine.createDemoGrid();
		Log::Logger::info("Demo grid reloaded.");
	} else {
		auto status = engine.loadGrid(args[0]);
		if (!status.isSuccess()) {
			Log::Logger::error(status.message);
			return;
		}
	}

	if (!engine.getSubstations().empty())
		Grid::Builder::BuilderShell::setActiveSubstation(
			engine.getSubstations().front());
}

} // namespace GLStation::IO::Commands::Builder

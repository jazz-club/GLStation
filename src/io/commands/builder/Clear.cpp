#include "grid/builder/Builder.hpp"
#include "io/commands/BuilderCommands.hpp"
#include "io/handlers/InputHandler.hpp"
#include "log/Diagnostics.hpp"
#include "log/Logger.hpp"
#include "sim/Engine.hpp"
#include <filesystem>

namespace GLStation::IO::Commands::Builder {

void cmdClear(Simulation::Engine &engine,
			  const std::vector<std::string> &args) {
	(void)args;

	if (!IO::InputHandler::confirmAction("Generative actions require the grid "
										 "and log file to be reset, continue?"))
		return;

	std::filesystem::remove("grid.csv");
	std::filesystem::remove("log.csv");
	Log::Diagnostics::clear();
	engine.getEventManager().clear();

	engine.clearSubstations();
	Grid::Builder::BuilderShell::setActiveSubstation(nullptr);
	Log::Logger::info("Cleared all substations.");
}

} // namespace GLStation::IO::Commands::Builder

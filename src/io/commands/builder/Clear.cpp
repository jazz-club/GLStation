#include "grid/builder/Builder.hpp"
#include "io/commands/BuilderCommands.hpp"
#include "log/Logger.hpp"
#include "sim/Engine.hpp"

namespace GLStation::IO::Commands::Builder {

void cmdClear(Simulation::Engine &engine,
			  const std::vector<std::string> &args) {
	(void)args;
	engine.clearSubstations();
	Grid::Builder::BuilderShell::setActiveSubstation(nullptr);
	Log::Logger::info("Cleared all substations.");
}

} // namespace GLStation::IO::Commands::Builder

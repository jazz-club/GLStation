#include "grid/Substation.hpp"
#include "grid/builder/Builder.hpp"
#include "io/commands/BuilderCommands.hpp"
#include "log/Logger.hpp"
#include "sim/Engine.hpp"

namespace GLStation::IO::Commands::Builder {

void cmdUse(Simulation::Engine &engine, const std::vector<std::string> &args) {
	if (args.empty()) {
		Log::Logger::warn("Usage: use <substation_name>");
		return;
	}
	for (auto &sub : engine.getSubstations()) {
		if (sub->getName() == args[0]) {
			Grid::Builder::BuilderShell::setActiveSubstation(sub);
			return;
		}
	}
	Log::Logger::error("Substation '" + args[0] + "' not found.");
}

} // namespace GLStation::IO::Commands::Builder

#include "io/commands/Commands.hpp"
#include "log/Logger.hpp"
#include "sim/Engine.hpp"
#include <iostream>

namespace GLStation::IO::Commands {

void cmdExport(Simulation::Engine &engine,
			   const std::vector<std::string> &args) {
	std::string filename = args.empty() ? "gls.csv" : args[0];
	engine.exportVoltagesToCSV(filename);
	Log::Logger::info("Exported to " + filename);
}

} // namespace GLStation::IO::Commands

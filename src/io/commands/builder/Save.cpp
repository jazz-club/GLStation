#include "io/commands/BuilderCommands.hpp"
#include "log/Logger.hpp"
#include "sim/Engine.hpp"

namespace GLStation::IO::Commands::Builder {

void cmdSave(Simulation::Engine &engine, const std::vector<std::string> &args) {
	if (args.empty()) {
		Log::Logger::warn("Usage: save <filename.csv>");
		return;
	}
	auto status = engine.saveGrid(args[0]);
	if (!status.isSuccess())
		Log::Logger::error(status.message);
}

} // namespace GLStation::IO::Commands::Builder

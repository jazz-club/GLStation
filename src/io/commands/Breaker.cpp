#include "grid/Breaker.hpp"
#include "io/commands/Commands.hpp"
#include "log/Logger.hpp"
#include "sim/Engine.hpp"
#include <iostream>

namespace GLStation::IO::Commands {

static void toggleBreaker(Simulation::Engine &engine,
						  const std::vector<std::string> &args, bool open) {
	if (args.empty()) {
		Log::Logger::warn(std::string("Usage: ") + (open ? "open" : "close") +
						  " <id>");
		return;
	}
	Core::u64 id;
	try {
		id = std::stoull(args[0]);
	} catch (...) {
		Log::Logger::error("Invalid ID.");
		return;
	}

	auto *comp = engine.findComponentById(id);
	if (!comp) {
		Log::Logger::error("No component with ID " + args[0]);
		return;
	}
	auto *breaker = dynamic_cast<Grid::Breaker *>(comp);
	if (!breaker) {
		Log::Logger::error("ID " + args[0] + " is not a breaker.");
		return;
	}
	breaker->setOpen(open);
	std::cout << "Breaker #" << id << " (" << comp->getName() << ") is now "
			  << (breaker->isOpen() ? "OPEN" : "CLOSED") << std::endl;
}

void cmdOpen(Simulation::Engine &engine, const std::vector<std::string> &args) {
	toggleBreaker(engine, args, true);
}

void cmdClose(Simulation::Engine &engine,
			  const std::vector<std::string> &args) {
	toggleBreaker(engine, args, false);
}

} // namespace GLStation::IO::Commands

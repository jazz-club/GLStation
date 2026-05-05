#include "grid/Breaker.hpp"
#include "grid/Generator.hpp"
#include "grid/Line.hpp"
#include "grid/Load.hpp"
#include "grid/Node.hpp"
#include "grid/Transformer.hpp"
#include "io/commands/Commands.hpp"
#include "io/handlers/InputHandler.hpp"
#include "log/Logger.hpp"
#include "sim/Engine.hpp"
#include <iostream>

namespace GLStation::IO::Commands {

void cmdSet(Simulation::Engine &engine, const std::vector<std::string> &args) {
	if (args.size() < 3) {
		Log::Logger::warn("Usage: set <id> <param> <value>");
		return;
	}
	Core::u64 id;
	double val;
	try {
		id = std::stoull(args[0]);
		val = std::stod(args[2]);
	} catch (...) {
		Log::Logger::error("Invalid ID or value.");
		return;
	}
	std::string param = InputHandler::normaliseForComparison(args[1]);

	auto *comp = engine.findComponentById(id);
	if (!comp) {
		Log::Logger::error("No component with ID " + args[0]);
		return;
	}

	bool ok = false;
	if (auto gen = dynamic_cast<Grid::Generator *>(comp)) {
		if (param == "p") {
			gen->setTargetP(val);
			ok = true;
		} else if (param == "v") {
			gen->setTargetV(val);
			ok = true;
		}
	} else if (auto load = dynamic_cast<Grid::Load *>(comp)) {
		if (param == "pf") {
			load->setPowerFactor(val);
			ok = true;
		} else if (param == "p") {
			load->setMaxPower(val);
			ok = true;
		}
	} else if (auto line = dynamic_cast<Grid::Line *>(comp)) {
		if (param == "limit") {
			line->setCurrentLimit(val);
			ok = true;
		}
	} else if (auto trafo = dynamic_cast<Grid::Transformer *>(comp)) {
		if (param == "limit") {
			trafo->setCurrentLimit(val);
			ok = true;
		} else if (param == "tap") {
			trafo->setTap(val);
			ok = true;
		}
	}

	if (ok) {
		Log::Logger::info("Set " + comp->getName() + " " + param + " = " +
						  std::to_string(val));
	} else {
		Log::Logger::error(
			"Parameter '" + param +
			"' not valid for this component. (gen: p,v; load: p,pf; "
			"line/trafo: limit; trafo: tap)");
	}
}

} // namespace GLStation::IO::Commands

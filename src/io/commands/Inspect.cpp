#include "grid/Breaker.hpp"
#include "grid/Generator.hpp"
#include "grid/Line.hpp"
#include "grid/Load.hpp"
#include "grid/Node.hpp"
#include "grid/Transformer.hpp"
#include "io/commands/Commands.hpp"
#include "log/Logger.hpp"
#include "sim/Engine.hpp"
#include "ui/Theme.hpp"
#include <iostream>

namespace GLStation::IO::Commands {

void cmdInspect(Simulation::Engine &engine,
				const std::vector<std::string> &args) {
	if (args.empty()) {
		Log::Logger::warn("Usage: inspect <id>");
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
		Log::Logger::error("No component with matching ID.");
		return;
	}

	std::cout << "\n"
			  << UI::Theme::cyan()
			  << "--- Component Inspect: " << comp->getName() << " ---"
			  << UI::Theme::reset() << std::endl;
	std::cout << "Type:     " << typeid(*comp).name() << std::endl;
	std::cout << "Summary:  " << comp->toString() << std::endl;

	auto fmtInspectKw = [](double kw) -> std::string {
		std::ostringstream o;
		o << std::fixed << std::setprecision(1);
		if (std::abs(kw) >= 1e6)
			o << (kw / 1e6) << " GW";
		else if (std::abs(kw) >= 1e3)
			o << (kw / 1e3) << " MW";
		else
			o << kw << " kW";
		return o.str();
	};

	if (auto line = dynamic_cast<Grid::Line *>(comp)) {
		std::cout << "Details:  Limit=" << line->getCurrentLimit() << " A"
				  << std::endl;
	} else if (auto gen = dynamic_cast<Grid::Generator *>(comp)) {
		std::cout << "Details:  P_target=" << fmtInspectKw(gen->getTargetP())
				  << ", V_target=" << gen->getTargetV() << " pu" << std::endl;
	} else if (auto load = dynamic_cast<Grid::Load *>(comp)) {
		std::cout << "Details:  P_max=" << fmtInspectKw(load->getMaxPower())
				  << ", PF=" << load->getPowerFactor() << std::endl;
	}
}

} // namespace GLStation::IO::Commands

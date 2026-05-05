#include "io/commands/Commands.hpp"
#include "sim/Engine.hpp"
#include "ui/Theme.hpp"
#include <iomanip>
#include <iostream>

namespace GLStation::IO::Commands {

void cmdStatus(Simulation::Engine &engine,
			   const std::vector<std::string> &args) {
	(void)args;
	std::cout << "\n"
			  << UI::Theme::cyan() << "--- System Overview ---"
			  << UI::Theme::reset() << std::endl;
	std::cout << "Frequency:   " << std::setw(8) << std::fixed
			  << std::setprecision(3) << engine.getSystemFrequency() << " Hz"
			  << std::endl;
	std::cout << "Generation:  " << std::setw(8) << std::fixed
			  << std::setprecision(1) << engine.getTotalGeneration() << " kW"
			  << std::endl;
	std::cout << "Load:        " << std::setw(8) << engine.getTotalLoad()
			  << " kW" << std::endl;
	std::cout << "Losses:      " << std::setw(8) << engine.getTotalLosses()
			  << " kW ("
			  << (engine.getTotalGeneration() > 0
					  ? (engine.getTotalLosses() / engine.getTotalGeneration() *
						 100)
					  : 0)
			  << "%)" << std::endl;
	std::cout << "Imbalance:   " << std::setw(8)
			  << (engine.getTotalGeneration() - engine.getTotalLoad() -
				  engine.getTotalLosses())
			  << " kW" << std::endl;
	std::cout << "Freq Nadir:  " << std::setw(8)
			  << engine.getFrequencyNadirLifetime() << " Hz" << std::endl;
	std::cout << "Max line:    " << std::setw(8)
			  << engine.getMaxLineLoadingLifetime() << " %" << std::endl;
	std::cout << "Shed load:   " << std::setw(8) << engine.getActiveShedLoadKw()
			  << " kW" << std::endl;
	std::cout << "Reserve:     " << std::setw(8) << engine.getReserveMarginKw()
			  << " kW" << std::endl;
	std::cout << UI::Theme::cyan() << "-----------------------"
			  << UI::Theme::reset() << std::endl;
	for (const auto &sub : engine.getSubstations()) {
		std::cout << sub->toString() << std::endl;
	}
}

} // namespace GLStation::IO::Commands

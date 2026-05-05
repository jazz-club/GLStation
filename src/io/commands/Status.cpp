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
	std::cout << UI::Theme::cyan() << "Frequency:   " << UI::Theme::reset()
			  << std::setw(8) << std::fixed << std::setprecision(3)
			  << engine.getSystemFrequency() << " Hz" << std::endl;
	std::cout << UI::Theme::green() << "Generation:  " << UI::Theme::reset()
			  << std::setw(8) << std::fixed << std::setprecision(1)
			  << engine.getTotalGeneration() << " kW" << std::endl;
	std::cout << UI::Theme::yellow() << "Load:        " << UI::Theme::reset()
			  << std::setw(8) << engine.getTotalLoad() << " kW" << std::endl;
	std::cout << UI::Theme::red() << "Losses:      " << UI::Theme::reset()
			  << std::setw(8) << engine.getTotalLosses() << " kW ("
			  << (engine.getTotalGeneration() > 0
					  ? (engine.getTotalLosses() / engine.getTotalGeneration() *
						 100)
					  : 0)
			  << "%)" << std::endl;

	double imbalance = engine.getTotalGeneration() - engine.getTotalLoad() -
					   engine.getTotalLosses();
	std::cout << (imbalance < 0 ? UI::Theme::red()
								: (imbalance > 0 ? UI::Theme::green()
												 : UI::Theme::cyan()))
			  << "Imbalance:   " << UI::Theme::reset() << std::setw(8)
			  << imbalance << " kW" << std::endl;

	std::cout << UI::Theme::dim() << "Freq Nadir:  " << UI::Theme::reset()
			  << std::setw(8) << engine.getFrequencyNadirLifetime() << " Hz"
			  << std::endl;
	std::cout << UI::Theme::dim() << "Max line:    " << UI::Theme::reset()
			  << std::setw(8) << engine.getMaxLineLoadingLifetime() << " %"
			  << std::endl;
	std::cout << UI::Theme::red() << "Shed load:   " << UI::Theme::reset()
			  << std::setw(8) << engine.getActiveShedLoadKw() << " kW"
			  << std::endl;
	std::cout << UI::Theme::green() << "Reserve:     " << UI::Theme::reset()
			  << std::setw(8) << engine.getReserveMarginKw() << " kW"
			  << std::endl;
	std::cout << UI::Theme::cyan() << "-----------------------"
			  << UI::Theme::reset() << std::endl;
	for (const auto &sub : engine.getSubstations()) {
		std::cout << sub->toString() << std::endl;
	}
}

} // namespace GLStation::IO::Commands

#include "grid/Node.hpp"
#include "io/commands/Commands.hpp"
#include "sim/Engine.hpp"
#include "ui/Theme.hpp"
#include <iomanip>
#include <iostream>

namespace GLStation::IO::Commands {

void cmdStatus(Simulation::Engine &engine,
			   const std::vector<std::string> &args) {
	(void)args;
	auto fmtStatusKw = [](double kw) -> std::string {
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

	double freq = engine.getSystemFrequency();
	double nominal = engine.getNominalHz();
	double gen = engine.getTotalGeneration();
	double load = engine.getTotalLoad();
	double losses = engine.getTotalLosses();
	double imbalance = gen - load - losses;
	double nadir = engine.getFrequencyNadirLifetime();
	double maxLine = engine.getMaxLineLoadingLifetime();
	double shed = engine.getActiveShedLoadKw();
	double reserve = engine.getReserveMarginKw();
	double rocof = engine.getRocofHzPerS();
	double agc = engine.getAgcIntegralMw();
	double totCap = engine.getTotalBatteryCapacity();
	double totCharge = engine.getTotalBatteryCharge();
	double soc = totCap > 1e-6 ? (totCharge / totCap * 100.0) : 0.0;

	int vViolations = 0;
	for (const auto &sub : engine.getSubstations()) {
		for (const auto &comp : sub->getComponents()) {
			if (auto node = dynamic_cast<Grid::Node *>(comp.get())) {
				double v = std::abs(node->getVoltage());
				if (node->isEnergised() && (v < 0.95 || v > 1.05))
					vViolations++;
			}
		}
	}

	std::cout << "\n"
			  << UI::Theme::cyan() << "--- System Overview ---"
			  << UI::Theme::reset() << std::endl;

	std::string fCol =
		(std::abs(freq - nominal) < 0.2)
			? UI::Theme::green()
			: (std::abs(freq - nominal) < 0.5 ? UI::Theme::yellow()
											  : UI::Theme::red());
	std::cout << "Frequency:    " << fCol << std::setw(12) << std::fixed
			  << std::setprecision(3) << freq << " Hz" << UI::Theme::reset()
			  << std::endl;

	std::cout << "Generation:   " << UI::Theme::green() << std::setw(12)
			  << fmtStatusKw(gen) << UI::Theme::reset() << std::endl;
	std::cout << "Load:         " << UI::Theme::yellow() << std::setw(12)
			  << fmtStatusKw(load) << UI::Theme::reset() << std::endl;

	double lossPct = (gen > 0) ? (losses / gen * 100.0) : 0.0;
	std::string lCol = (lossPct < 5.0) ? UI::Theme::reset()
									   : (lossPct < 10.0 ? UI::Theme::yellow()
														 : UI::Theme::red());
	std::cout << "Losses:       " << lCol << std::setw(12)
			  << fmtStatusKw(losses) << " (" << std::fixed
			  << std::setprecision(1) << lossPct << "%)" << UI::Theme::reset()
			  << std::endl;

	std::string iCol =
		(std::abs(imbalance) < 1.0) ? UI::Theme::green() : UI::Theme::red();
	std::cout << "Imbalance:    " << iCol << std::setw(12)
			  << fmtStatusKw(imbalance) << UI::Theme::reset() << std::endl;

	std::string nCol =
		(nadir > nominal - 0.5)
			? UI::Theme::green()
			: (nadir > nominal - 1.0 ? UI::Theme::yellow() : UI::Theme::red());
	std::cout << "Freq Nadir:   " << nCol << std::setw(12) << std::fixed
			  << std::setprecision(3) << nadir << " Hz" << UI::Theme::reset()
			  << std::endl;

	std::string mlCol =
		(maxLine < 80.0)
			? UI::Theme::green()
			: (maxLine < 100.0 ? UI::Theme::yellow() : UI::Theme::red());
	std::cout << "Max Line:     " << mlCol << std::setw(12) << std::fixed
			  << std::setprecision(1) << maxLine << " %" << UI::Theme::reset()
			  << std::endl;

	std::string sCol = (shed > 0.1) ? UI::Theme::red() : UI::Theme::green();
	std::cout << "Shed Load:    " << sCol << std::setw(12) << fmtStatusKw(shed)
			  << UI::Theme::reset() << std::endl;

	double resReq = load * 0.1;
	std::string rCol = (reserve > resReq) ? UI::Theme::green()
										  : (reserve > 0.1 ? UI::Theme::yellow()
														   : UI::Theme::red());
	std::cout << "Reserve:      " << rCol << std::setw(12)
			  << fmtStatusKw(reserve) << UI::Theme::reset() << std::endl;

	std::string roCol =
		(std::abs(rocof) < 0.1)
			? UI::Theme::reset()
			: (std::abs(rocof) < 0.5 ? UI::Theme::yellow() : UI::Theme::red());
	std::cout << "ROCOF:        " << roCol << std::setw(12) << std::fixed
			  << std::setprecision(3) << rocof << " Hz/s" << UI::Theme::reset()
			  << std::endl;

	std::cout << "AGC Status:   " << UI::Theme::cyan() << std::setw(12)
			  << std::fixed << std::setprecision(1) << agc << " MW"
			  << UI::Theme::reset() << std::endl;

	std::string socCol =
		(soc < 20.0) ? UI::Theme::red()
					 : (soc < 50.0 ? UI::Theme::yellow() : UI::Theme::green());
	std::cout << "Storage SOC:  "
			  << (totCap > 1e-6 ? socCol : UI::Theme::reset()) << std::setw(12)
			  << std::fixed << std::setprecision(1) << soc << " %"
			  << UI::Theme::reset() << std::endl;

	std::string vCol =
		(vViolations > 0) ? UI::Theme::red() : UI::Theme::green();
	std::cout << "V Violations: " << vCol << std::setw(12) << vViolations
			  << UI::Theme::reset() << std::endl;

	std::cout << UI::Theme::cyan() << "-----------------------"
			  << UI::Theme::reset() << std::endl;

	for (const auto &sub : engine.getSubstations()) {
		std::cout << sub->toString() << std::endl;
	}
}

} // namespace GLStation::IO::Commands

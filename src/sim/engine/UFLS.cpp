#include "grid/Load.hpp"
#include "sim/Engine.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>

namespace GLStation::Simulation {

void Engine::initialiseUflsStages() {
	m_uflsStages.clear();
	m_uflsStages.push_back({49.0, 0.10, 0, 1e9, false, false, 0});
	m_uflsStages.push_back({48.7, 0.15, 0, 1e9, false, false, 0});
	m_uflsStages.push_back({48.4, 0.20, 0, 1e9, false, false, 0});
	m_uflsStages.push_back({48.0, 0.30, 0, 1e9, false, false, 0});
}

/*
		load shreading thresholds
		erik you probably understand 

		sources;
		https://www.aemo.com.au/energy-systems/electricity/wholesale-electricity-market-wem/system-operations/under-frequency-load-shedding
		https://www.sciencedirect.com/science/article/pii/S1364032123001508
*/

void Engine::processUFLS() {
	for (auto &stage : m_uflsStages) {
		bool rocoOk =
			stage.rocoThreshold > 1e8 || m_rocofHzPerS <= -stage.rocoThreshold;
		if (!stage.triggered && m_systemFrequency <= stage.freqThresholdHz &&
			rocoOk) {
			if (!stage.armPending) {
				stage.armPending = true;
				stage.armAtTick = m_currentTick + stage.delayTicks;
			}
			if (stage.armPending && m_currentTick >= stage.armAtTick) {
				stage.triggered = true;
				stage.armPending = false;

				std::vector<Grid::Load *> activLoads;
				for (auto &sub : m_substations)
					for (auto &comp : sub->getComponents())
						if (auto load = dynamic_cast<Grid::Load *>(comp.get()))
							if (!load->isShed())
								activLoads.push_back(load);

				size_t nShed = static_cast<size_t>(
					static_cast<Core::f64>(activLoads.size()) *
					stage.shedFraction);
				std::sort(activLoads.begin(), activLoads.end(),
						  [](auto *a, auto *b) {
							  return a->getCurrentConsumption() >
									 b->getCurrentConsumption();
						  });

				std::ostringstream os;
				os << "[UFLS] " << std::fixed << std::setprecision(2)
				   << m_systemFrequency << " Hz <= " << stage.freqThresholdHz
				   << " Hz. Shedding " << nShed << " loads.";
				logEvent(os.str(), Log::Severity::Critical);

				for (size_t i = 0; i < nShed && i < activLoads.size(); ++i)
					activLoads[i]->shed();
			}
		} else if (m_systemFrequency > stage.freqThresholdHz + 0.02) {
			stage.armPending = false;
		}

		if (stage.triggered &&
			m_systemFrequency > stage.freqThresholdHz + 0.6) {
			stage.triggered = false;
			size_t restored = 0;
			for (auto &sub : m_substations) {
				for (auto &comp : sub->getComponents()) {
					if (auto load = dynamic_cast<Grid::Load *>(comp.get())) {
						if (load->isShed() && restored < 2) {
							load->restore();
							++restored;
						}
					}
				}
			}
			if (restored > 0)
				logEvent("[UFLS] Frequency recovered, restoring staged load.",
						 Log::Severity::Critical);
		}
	}
}

} // namespace GLStation::Simulation

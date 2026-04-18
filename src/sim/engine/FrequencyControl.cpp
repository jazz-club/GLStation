#include "grid/Generator.hpp"
#include "sim/Engine.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace GLStation::Simulation {

/*
		hardcoded to 50hz still
		df/dt ∝ (P_gen - P_load - P_loss) / (2HSbase), range is 49-51(?) 
*/
void Engine::updateFrequencyDynamics() {
	Core::f64 totalKE = 0.0;
	for (auto &sub : m_substations) {
		for (auto &comp : sub->getComponents()) {
			if (auto gen = dynamic_cast<Grid::Generator *>(comp.get()))
				totalKE +=
					std::max(0.1, gen->getInertiaH()) * gen->getMaxPower();
		}
	}
	if (totalKE < 1e-6)
		totalKE = 500000.0;
	const Core::f64 D = 0.08;
	Core::f64 dt = std::chrono::duration<Core::f64>(m_simStep).count() / 1000.0;

	Core::f64 imbalance = m_totalGeneration - m_totalLoad - m_totalLosses;
	Core::f64 df_dt = (imbalance * m_nominalHz) / (2.0 * totalKE) -
					  D * (m_systemFrequency - m_nominalHz);
	m_systemFrequency += df_dt * dt;
	m_systemFrequency =
		std::clamp(m_systemFrequency, m_nominalHz - 3.0, m_nominalHz + 2.0);
}

/*
		-df * gain for slope across gens
		i might be misunderstanding the underlying logic 
		https://www.academia.edu/32675657/Automatic_Gain_Control_AGC_in_Receivers
*/
void Engine::applyAGC() {
	Core::f64 dt = std::chrono::duration<Core::f64>(m_simStep).count() / 1000.0;
	Core::f64 ace = -(m_systemFrequency - m_nominalHz);
	if (std::abs(ace) < 0.005)
		return;

	Core::f64 totalCapacity = 0.0;
	for (auto &sub : m_substations)
		for (auto &comp : sub->getComponents())
			if (auto gen = dynamic_cast<Grid::Generator *>(comp.get()))
				if (gen->getMode() != Grid::GeneratorMode::Slack)
					totalCapacity += gen->getMaxPower();

	if (totalCapacity < 1e-6)
		return;

	m_agcIntegralMw += ace * dt;
	m_agcIntegralMw = std::clamp(m_agcIntegralMw, -50.0, 50.0);

	Core::f64 Kp = totalCapacity * 0.00005;
	Core::f64 Ki = totalCapacity * 0.00001;
	Core::f64 maxCorr = totalCapacity * 0.001;
	Core::f64 totalCorrection =
		std::clamp(-ace * Kp - m_agcIntegralMw * Ki, -maxCorr, maxCorr);

	if (std::abs(totalCorrection) > 500.0 && m_currentTick % 8000 == 0) {
		std::ostringstream os;
		os << "[AGC] Dispatched secondary governor adjustment: " << std::fixed
		   << std::setprecision(1) << (totalCorrection > 0 ? "+" : "")
		   << totalCorrection << "kW";
		logEvent(os.str());
	}

	for (auto &sub : m_substations) {
		for (auto &comp : sub->getComponents()) {
			if (auto gen = dynamic_cast<Grid::Generator *>(comp.get())) {
				if (gen->getMode() != Grid::GeneratorMode::Slack) {
					Core::f64 share = gen->getMaxPower() / totalCapacity;
					gen->adjustSetpointKw(totalCorrection * share);
				}
			}
		}
	}
}

} // namespace GLStation::Simulation

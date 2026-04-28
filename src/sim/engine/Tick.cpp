#include "grid/Generator.hpp"
#include "grid/Line.hpp"
#include "grid/Load.hpp"
#include "grid/Transformer.hpp"
#include "sim/Engine.hpp"
#include "sim/PowerSolver.hpp"
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace GLStation::Simulation {

static SimTickState g_simTickState{};

SimTickState &Engine::simTickState() { return g_simTickState; }

Engine::Engine()
	: m_currentTick(0), m_simTime(std::chrono::milliseconds{0}),
	  m_simStep(std::chrono::milliseconds{1}), m_nominalHz(50.0),
	  m_lastFreqHz(50.0), m_agcIntegralMw(0.0), m_rocofHzPerS(0.0),
	  m_systemFrequency(50.0), m_totalGeneration(0), m_totalLoad(0),
	  m_totalLosses(0), m_frequencyNadir(50.0), m_frequencyNadirLifetime(50.0),
	  m_maxObservedLineLoadingPercent(0.0), m_maxLineLoadingLifetime(0.0),
	  m_activeShedLoadKw(0.0), m_reserveMarginKw(0.0) {}

void Engine::pushSimTickState() {
	g_simTickState.simTime = m_simTime;
	g_simTickState.step = m_currentTick;
	g_simTickState.nominalHz = m_nominalHz;
	g_simTickState.systemHz = m_systemFrequency;
	g_simTickState.dtSeconds =
		std::chrono::duration<Core::f64>(m_simStep).count() / 1000.0;
}

/*
		tick manager, need to add debug lines per component
		most of the logic here is self explanatory because of the component names
		slack and loading balacing and AGC trigger at the end of every tick, probably not the most optimised solution
*/
void Engine::tick() {
	m_currentTick++;
	m_simTime += m_simStep;
	pushSimTickState();
	m_scenarioManager.update(m_currentTick);

	m_totalLoad = 0.0;
	for (auto &sub : m_substations) {
		sub->tick(m_currentTick);
		for (auto &comp : sub->getComponents()) {
			if (auto load = dynamic_cast<Grid::Load *>(comp.get())) {
				m_totalLoad += load->getCurrentConsumption();
			}
		}
	}

	for (auto &sub : m_substations) {
		for (auto &comp : sub->getComponents()) {
			if (auto gen = dynamic_cast<Grid::Generator *>(comp.get())) {
				gen->applyDroopResponse(m_systemFrequency, m_nominalHz);
				gen->stepGovernor(g_simTickState.dtSeconds);
			}
		}
	}

	PowerSolver::solve(m_substations, SolverSettings());

	for (auto &sub : m_substations) {
		for (auto &comp : sub->getComponents()) {
			if (auto line = dynamic_cast<Grid::Line *>(comp.get()))
				line->tick(m_currentTick);
			if (auto trafo = dynamic_cast<Grid::Transformer *>(comp.get()))
				trafo->tick(m_currentTick);
		}
	}

	m_totalGeneration = 0.0;
	m_totalLosses = 0.0;
	for (auto &sub : m_substations) {
		for (auto &comp : sub->getComponents()) {
			if (auto gen = dynamic_cast<Grid::Generator *>(comp.get()))
				m_totalGeneration += gen->getActualP();
			if (auto line = dynamic_cast<Grid::Line *>(comp.get()))
				m_totalLosses += line->getLosses();
			if (auto trafo = dynamic_cast<Grid::Transformer *>(comp.get()))
				m_totalLosses += trafo->getLosses();
		}
	}

	updateKpis();
	Core::f64 dt = std::chrono::duration<Core::f64>(m_simStep).count() / 1000.0;
	if (dt > 1e-9)
		m_rocofHzPerS = (m_systemFrequency - m_lastFreqHz) / dt;
	m_lastFreqHz = m_systemFrequency;
	updateFrequencyDynamics();
	applyAGC();
	for (auto &sub : m_substations) {
		for (auto &comp : sub->getComponents()) {
			if (auto gen = dynamic_cast<Grid::Generator *>(comp.get()))
				gen->stepExciter(g_simTickState.dtSeconds);
		}
	}
	processProtectionRelays();
	updateKpis();

	if (m_currentTick > 0 && m_currentTick % 25000 == 0) {
		std::ostringstream os;
		os << "Grid Envelope Normal: " << std::fixed << std::setprecision(2)
		   << (m_totalGeneration / 1000.0) << "MW Generation, "
		   << (m_totalLoad / 1000.0) << "MW Demand. Frequency "
		   << m_systemFrequency << "Hz.";
		logEvent(os.str());
	}
}

} // namespace GLStation::Simulation

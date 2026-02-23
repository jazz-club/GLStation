#pragma once

#include "grid/Substation.hpp"
#include "sim/ScenarioManager.hpp"
#include <filesystem>
#include <memory>
#include <vector>

namespace GLStation::Simulation {

class Engine {
  public:
	Engine();

	void initialise();
	void tick();

	const std::vector<std::shared_ptr<Grid::Substation>> &
	getSubstations() const {
		return m_substations;
	}
	void createDemoGrid();
	Core::Tick getTickCount() const { return m_currentTick; }
	ScenarioManager &getScenarioManager() { return m_scenarioManager; }
	Core::f64 getSystemFrequency() const { return m_systemFrequency; }
	Core::f64 getTotalLoad() const { return m_totalLoad; }
	Core::f64 getTotalGeneration() const { return m_totalGeneration; }
	Core::f64 getTotalLosses() const { return m_totalLosses; }
	bool openBreakerById(Core::u64 id);
	bool closeBreakerById(Core::u64 id);
	bool setLoadPowerById(Core::u64 id, Core::f64 powerKw);
	bool setGenTargetPById(Core::u64 id, Core::f64 powerKw);
	void exportVoltagesToCSV(const std::string &filename);

  private:
	void processProtectionRelays();
	void updateFrequencyDynamics();
	void applyAGC();
	Core::Tick m_currentTick;
	std::vector<std::shared_ptr<Grid::Substation>> m_substations;
	ScenarioManager m_scenarioManager;
	Core::f64 m_systemFrequency;
	Core::f64 m_totalGeneration;
	Core::f64 m_totalLoad;
	Core::f64 m_totalLosses;
};

} // namespace GLStation::Simulation

#pragma once

#include "grid/Substation.hpp"
#include "sim/ScenarioManager.hpp"
#include <deque>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
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
	/*
	This is FUCKED by default btw
	plan is to create demo scenario strengths for the imported cities 
	maybe just limit to a few demo cities until we have a better city builder?
*/
	std::vector<std::string> getDemoScenarioNames() const;
	bool runDemoScenario(const std::string &name);
	void clearScheduledEvents();
	void clearEventLog();
	const std::deque<std::string> &getRecentEvents() const {
		return m_eventLog;
	}
	std::string getLastEvent() const;
	Core::f64 getFrequencyNadir() const { return m_frequencyNadir; }
	Core::f64 getMaxObservedLineLoadingPercent() const {
		return m_maxObservedLineLoadingPercent;
	}
	Core::f64 getActiveShedLoadKw() const { return m_activeShedLoadKw; }
	Core::f64 getReserveMarginKw() const { return m_reserveMarginKw; }
	bool runDeterministicDemoValidation(std::string &report);

  private:
	void processProtectionRelays();
	void updateFrequencyDynamics();
	void applyAGC();
	void configureDemoProfiles();
	void logEvent(const std::string &event);
	void appendEventToCsv(Core::Tick tick, const std::string &event);
	void updateKpis();
	Core::Tick m_currentTick;
	std::vector<std::shared_ptr<Grid::Substation>> m_substations;
	ScenarioManager m_scenarioManager;
	Core::f64 m_systemFrequency;
	Core::f64 m_totalGeneration;
	Core::f64 m_totalLoad;
	Core::f64 m_totalLosses;
	std::deque<std::string> m_eventLog;
	std::map<Core::u64, Core::u64> m_pendingTrips;
	std::map<Core::u64, Core::u64> m_recloseAtTick;
	Core::f64 m_frequencyNadir;
	Core::f64 m_maxObservedLineLoadingPercent;
	Core::f64 m_activeShedLoadKw;
	Core::f64 m_reserveMarginKw;
};

} // namespace GLStation::Simulation

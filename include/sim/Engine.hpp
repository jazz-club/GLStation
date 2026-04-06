#pragma once

#include "grid/Substation.hpp"
#include "sim/ScenarioManager.hpp"
#include <chrono>
#include <deque>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace GLStation::Simulation {

struct SimTickState {
	std::chrono::milliseconds simTime{};
	Core::Tick step{};
	Core::f64 nominalHz{50.0};
	Core::f64 systemHz{50.0};
	Core::f64 dtSeconds{0.001};
};

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
	std::chrono::milliseconds getSimTime() const { return m_simTime; }
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
	void setNominalHz(Core::f64 hz) { m_nominalHz = hz; }
	Core::f64 getNominalHz() const { return m_nominalHz; }
	Core::f64 getAgcIntegralMw() const { return m_agcIntegralMw; }
	Core::f64 getRocofHzPerS() const { return m_rocofHzPerS; }
	static SimTickState &simTickState();
	const std::deque<std::string> &getRecentEvents() const {
		return m_eventLog;
	}
	std::string getLastEvent() const;
	Core::f64 getFrequencyNadir() const { return m_frequencyNadir; }
	Core::f64 getFrequencyNadirLifetime() const {
		return m_frequencyNadirLifetime;
	}
	Core::f64 getMaxObservedLineLoadingPercent() const {
		return m_maxObservedLineLoadingPercent;
	}
	Core::f64 getMaxLineLoadingLifetime() const {
		return m_maxLineLoadingLifetime;
	}
	Core::f64 getActiveShedLoadKw() const { return m_activeShedLoadKw; }
	Core::f64 getReserveMarginKw() const { return m_reserveMarginKw; }

  private:
	void processProtectionRelays();
	void updateFrequencyDynamics();
	void applyAGC();
	void configureDemoProfiles();
	void logEvent(const std::string &event);
	void appendEventToCsv(Core::Tick tick, const std::string &event);
	void updateKpis();
	void loadUflsFile();
	void pushSimTickState();
	Core::Tick m_currentTick;
	std::chrono::milliseconds m_simTime;
	std::chrono::milliseconds m_simStep;
	Core::f64 m_nominalHz;
	Core::f64 m_lastFreqHz;
	Core::f64 m_agcIntegralMw;
	Core::f64 m_rocofHzPerS;
	std::vector<std::shared_ptr<Grid::Substation>> m_substations;
	ScenarioManager m_scenarioManager;
	Core::f64 m_systemFrequency;
	Core::f64 m_totalGeneration;
	Core::f64 m_totalLoad;
	Core::f64 m_totalLosses;
	std::deque<std::string> m_eventLog;
	std::map<Core::u64, Core::u64> m_pendingTrips;
	std::map<Core::u64, Core::u64> m_overloadStartTick;
	std::map<Core::u64, Core::u64> m_recloseAtTick;
	std::map<Core::u64, Core::u64> m_recloseCooldownUntil;
	std::map<Core::u64, Core::u32> m_recloseLockout;
	Core::f64 m_frequencyNadir;
	Core::f64 m_frequencyNadirLifetime;
	Core::f64 m_maxObservedLineLoadingPercent;
	Core::f64 m_maxLineLoadingLifetime;
	Core::f64 m_activeShedLoadKw;
	Core::f64 m_reserveMarginKw;
	struct UflsStageCfg {
		Core::f64 freqThresholdHz;
		Core::f64 shedFraction;
		Core::u64 delayTicks;
		Core::f64 rocoThreshold;
		bool triggered;
		bool armPending;
		Core::u64 armAtTick;
	};
	std::vector<UflsStageCfg> m_uflsStages;
};

} // namespace GLStation::Simulation

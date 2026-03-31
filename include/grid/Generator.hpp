#pragma once

#include "grid/GridComponent.hpp"
#include "grid/Node.hpp"

namespace GLStation::Grid {

enum class GeneratorMode { Slack, PV, PQ };
enum class GeneratorProfile { Manual, Thermal, Hydro, Wind, Solar };

class Generator : public GridComponent {
  public:
	Generator(std::string name, Node *connectedNode, GeneratorMode mode);

	void tick(Core::Tick currentTick) override;
	std::string toString() const override;

	void setTargetP(Core::f64 pKw) {
		m_targetP = pKw;
		m_actualP = pKw;
	}
	void setTargetV(Core::f64 vPu) { m_targetV = vPu; }
	void setQLimits(Core::f64 minQ, Core::f64 maxQ) {
		m_minQ = minQ;
		m_maxQ = maxQ;
	}

	Core::f64 getTargetP() const { return m_targetP; }
	Core::f64 getActualP() const { return m_actualP; }
	Core::f64 getTargetV() const { return m_targetV; }
	GeneratorMode getMode() const { return m_mode; }
	Node *getConnectedNode() const { return m_connectedNode; }

	void applyDroopResponse(Core::f64 freqHz);

	void adjustSetpointKw(Core::f64 deltaKw) {
		m_targetP = std::max(0.0, m_targetP + deltaKw);
		if (m_mode != GeneratorMode::Slack)
			m_actualP = std::max(0.0, m_actualP + deltaKw);
	}

	/*
	"strength" scales output 
	roughtly accurate but droop needs to be adjusted 
	https://doi.org/10.29294/IJASE.9.4.2023.3121-3133
*/
	void setActualPowerKw(Core::f64 pKw) { m_actualP = pKw; }
	void setProfile(GeneratorProfile profile) { m_profile = profile; }
	GeneratorProfile getProfile() const { return m_profile; }
	void setProfileStrength(Core::f64 strength) {
		m_profileStrength = strength;
	}
	void setDroop(Core::f64 droop) { m_droop = droop; }
	void setMaxRampRate(Core::f64 rampKwPerTick) {
		m_maxRampRate = rampKwPerTick;
	}
	void setPowerBounds(Core::f64 pMin, Core::f64 pMax) {
		m_minP = std::max(0.0, pMin);
		m_maxP = std::max(m_minP, pMax);
	}
	Core::f64 getMinPower() const { return m_minP; }
	Core::f64 getMaxPower() const { return m_maxP; }

  private:
	Node *m_connectedNode;
	GeneratorMode m_mode;
	GeneratorProfile m_profile;
	Core::f64 m_targetP;
	Core::f64 m_actualP;
	Core::f64 m_targetV;
	Core::f64 m_minQ, m_maxQ;
	Core::f64 m_minP, m_maxP;
	Core::f64 m_droop;
	Core::f64 m_inertia;
	Core::f64 m_maxRampRate;
	Core::f64 m_profileStrength;
};

} // namespace GLStation::Grid

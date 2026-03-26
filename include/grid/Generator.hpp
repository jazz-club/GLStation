#pragma once

#include "grid/GridComponent.hpp"
#include "grid/Node.hpp"

namespace GLStation::Grid {

enum class GeneratorMode { Slack, PV, PQ };

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

	void setActualPowerKw(Core::f64 pKw) { m_actualP = pKw; }

  private:
	Node *m_connectedNode;
	GeneratorMode m_mode;
	Core::f64 m_targetP;
	Core::f64 m_actualP;
	Core::f64 m_targetV;
	Core::f64 m_minQ, m_maxQ;
	Core::f64 m_droop;
	Core::f64 m_inertia;
	Core::f64 m_maxRampRate;
};

} // namespace GLStation::Grid

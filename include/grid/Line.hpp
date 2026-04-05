#pragma once

#include "grid/GridComponent.hpp"
#include "grid/Node.hpp"
#include <complex>

namespace GLStation::Grid {

class Line : public GridComponent {
  public:
	Line(std::string name, Node *from, Node *to, Core::f64 r, Core::f64 x,
		 Core::f64 lineChargingSusceptancePu = 0.0);

	void tick(Core::Tick currentTick) override;
	std::string toString() const override;

	std::complex<Core::f64> getAdmittance() const { return m_admittance; }
	Node *getFromNode() const { return m_from; }
	Node *getToNode() const { return m_to; }
	Core::f64 getResistance() const { return m_resistance; }
	Core::f64 getReactance() const { return m_reactance; }
	void setResistanceReactance(Core::f64 r, Core::f64 x);
	void setLineChargingSusceptancePu(Core::f64 bTotalPu);
	Core::f64 getLineChargingSusceptancePu() const {
		return m_lineChargingSusceptancePu;
	}
	std::complex<Core::f64> getCurrentFlow() const { return m_currentFlow; }
	Core::f64 getCurrentLimit() const { return m_currentLimit; }
	void setCurrentLimit(Core::f64 limit) { m_currentLimit = limit; }
	Core::f64 getLosses() const {
		Core::f64 iMag = std::abs(m_currentFlow);
		return 3.0 * iMag * iMag * m_resistance / 1000.0;
	}
	void setFlowFromSolverPu(std::complex<Core::f64> seriesIpu,
							 Core::f64 rPuEff);
	void setDistanceRelay(Core::f64 reachZohm, bool enabled);
	bool isDistanceRelayEnabled() const { return m_distanceRelay; }
	Core::f64 getDistanceReachZohm() const { return m_distanceReachZohm; }
	Core::f64 getSeriesFlowPuMag() const { return std::abs(m_seriesFlowPu); }

  private:
	Node *m_from;
	Node *m_to;
	Core::f64 m_resistance;
	Core::f64 m_reactance;
	Core::f64 m_lineChargingSusceptancePu;
	Core::f64 m_currentLimit;
	std::complex<Core::f64> m_admittance;
	std::complex<Core::f64> m_currentFlow;
	std::complex<Core::f64> m_seriesFlowPu;
	bool m_distanceRelay;
	Core::f64 m_distanceReachZohm;
};

} // namespace GLStation::Grid

#pragma once

#include "grid/GridComponent.hpp"
#include "grid/Node.hpp"

namespace GLStation::Grid {

class Battery : public GridComponent {
  public:
	Battery(std::string name, Node *connectedNode, Core::f64 capacityKw,
			Core::f64 maxChargeRateKw, Core::f64 maxDischargeRateKw);

	void tick(Core::Tick currentTick) override;
	std::string toString() const override;

	Node *getConnectedNode() const { return m_connectedNode; }
	Core::f64 getCapacityKw() const { return m_capacityKw; }
	Core::f64 getChargeKw() const { return m_chargeKw; }
	Core::f64 getMaxChargeRateKw() const { return m_maxChargeRateKw; }
	Core::f64 getMaxDischargeRateKw() const { return m_maxDischargeRateKw; }

	void setChargeKw(Core::f64 charge) { m_chargeKw = charge; }

	Core::f64 getActivePowerKw() const { return m_activePowerKw; }
	void setActivePowerKw(Core::f64 pKw) { m_activePowerKw = pKw; }

	Core::f64 charge(Core::f64 powerKw, Core::f64 dtSec);
	Core::f64 discharge(Core::f64 powerKw, Core::f64 dtSec);

  private:
	Node *m_connectedNode;
	Core::f64 m_capacityKw;
	Core::f64 m_chargeKw;
	Core::f64 m_maxChargeRateKw;
	Core::f64 m_maxDischargeRateKw;
	Core::f64 m_activePowerKw{0.0};
};

} // namespace GLStation::Grid

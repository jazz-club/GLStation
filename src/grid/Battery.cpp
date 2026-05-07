#include "grid/Battery.hpp"
#include "ui/Theme.hpp"
#include <algorithm>
#include <sstream>

namespace GLStation::Grid {

Battery::Battery(std::string name, Node *connectedNode, Core::f64 capacityKw,
				 Core::f64 maxChargeRateKw, Core::f64 maxDischargeRateKw)
	: GridComponent(std::move(name)), m_connectedNode(connectedNode),
	  m_capacityKw(capacityKw), m_chargeKw(0.0),
	  m_maxChargeRateKw(maxChargeRateKw),
	  m_maxDischargeRateKw(maxDischargeRateKw) {}

void Battery::tick(Core::Tick currentTick) { (void)currentTick; }

std::string Battery::toString() const {
	std::ostringstream os;
	os << UI::Theme::green() << "Battery[" << UI::Theme::reset() << m_name
	   << UI::Theme::green() << "] " << UI::Theme::dim()
	   << "cap=" << UI::Theme::reset() << m_capacityKw << "kW "
	   << UI::Theme::dim() << "charge=" << UI::Theme::cyan() << m_chargeKw
	   << "kW" << UI::Theme::dim() << " maxC=" << UI::Theme::reset()
	   << m_maxChargeRateKw << "kW " << UI::Theme::dim()
	   << "maxD=" << UI::Theme::reset() << m_maxDischargeRateKw << "kW";
	return os.str();
}

Core::f64 Battery::charge(Core::f64 powerKw, Core::f64 dtSec) {
	Core::f64 actualPower = std::min(powerKw, m_maxChargeRateKw);
	Core::f64 energyKwSec = actualPower * dtSec;
	Core::f64 availableSpace = m_capacityKw - m_chargeKw;

	if (energyKwSec > availableSpace) {
		energyKwSec = availableSpace;
		actualPower = energyKwSec / dtSec;
	}

	m_chargeKw += energyKwSec;
	return actualPower;
}

Core::f64 Battery::discharge(Core::f64 powerKw, Core::f64 dtSec) {
	Core::f64 actualPower = std::min(powerKw, m_maxDischargeRateKw);
	Core::f64 energyKwSec = actualPower * dtSec;

	if (energyKwSec > m_chargeKw) {
		energyKwSec = m_chargeKw;
		actualPower = energyKwSec / dtSec;
	}

	m_chargeKw -= energyKwSec;
	return actualPower;
}

} // namespace GLStation::Grid

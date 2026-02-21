#include "grid/Transformer.hpp"
#include <cmath>
#include <iomanip>
#include <sstream>

/*
		connects grid components using series impedance
    	voltage loss is |I|^2 x R , |Z| ≤ ε, refer to grid/Line
		voltage is calculated by taking the current deviation multiplied by the admittance
*/

namespace GLStation::Grid {

/*
		calculate admittance by Z = r + jx, Y = 1/Z, inversion to prevent accidental divide by 0
*/
Transformer::Transformer(std::string name, Node *primary, Node *secondary,
						 Core::f64 r, Core::f64 x, Core::f64 tap)
	: GridComponent(std::move(name)), m_primary(primary),
	  m_secondary(secondary), m_resistance(r), m_reactance(x), m_tap(tap),
	  m_currentLimit(1000.0), m_admittance(0, 0), m_currentFlow(0, 0) {
	std::complex<Core::f64> z(r, x);
	if (std::abs(z) > Core::EPSILON) {
		m_admittance = 1.0 / z;
	}
}

/*
		pu × base, current available current is primary - secondary voltage normalised to Kv
*/
void Transformer::tick(Core::Tick) {
	if (!m_primary || !m_secondary)
		return;
	std::complex<Core::f64> vpKv =
		m_primary->getVoltage() * m_primary->getBaseVoltage();
	std::complex<Core::f64> vsKv =
		m_secondary->getVoltage() * m_secondary->getBaseVoltage();
	std::complex<Core::f64> vpAdjusted = vpKv / m_tap;
	std::complex<Core::f64> vDiff = vpAdjusted - vsKv;
	m_currentFlow = (vDiff * 1000.0) * m_admittance;
}

/*
		voltage loss = |I|² RW, converted to kW
*/
Core::f64 Transformer::getLosses() const {
	Core::f64 iMag = std::abs(m_currentFlow);
	return iMag * iMag * m_resistance / 1000.0;
}

std::complex<Core::f64> Transformer::getAdmittance() const {
	return std::complex<Core::f64>(1.0, 0.0) /
		   std::complex<Core::f64>(m_resistance, m_reactance);
}

std::string Transformer::toString() const {
	std::stringstream ss;
	ss << "[Transformer #" << m_id << "] " << m_name << " | "
	   << (m_primary ? m_primary->getName() : "N/A") << " ("
	   << (m_primary ? m_primary->getBaseVoltage() : 0.0) << "kV) -> "
	   << (m_secondary ? m_secondary->getName() : "N/A") << " ("
	   << (m_secondary ? m_secondary->getBaseVoltage() : 0.0)
	   << "kV) | Tap: " << std::fixed << std::setprecision(2) << m_tap
	   << ", Z: " << std::fixed << std::setprecision(3) << m_resistance << "+j"
	   << m_reactance;
	return ss.str();
}

} // namespace GLStation::Grid

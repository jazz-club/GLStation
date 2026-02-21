#include "grid/Line.hpp"
#include <iomanip>
#include <sstream>

namespace GLStation::Grid {
/*
    	receiver pathing, I = Y ΔV; losses P_Loss = |I|^2 R
    	useful for admittance matrix population in the power solver
    	since its lower level at line
    	Z=r + jx while Y = 1/Z as long as |Z| ≤ ε, prevents impedence rolling over
		to division by 0, forces impedence to 0 when needed current amp limit is set
		to 500amps, fix when get data
*/
Line::Line(std::string name, Node *from, Node *to, Core::f64 r, Core::f64 x)
	: GridComponent(std::move(name)), m_from(from), m_to(to), m_resistance(r),
	  m_reactance(x), m_currentLimit(500.0) {
	std::complex<Core::f64> impedance(r, x);
	if (std::abs(impedance) > Core::EPSILON) {
		m_admittance = 1.0 / impedance;
	} else {
		m_admittance = 0.0;
	}
}

/*
    	tick math, I = Y × (m_fromV − m_toV), I = ΔV x 1000 x Y, (kV is more human readable)
		so conversion and returns current in amps
*/
void Line::tick(Core::Tick) {
	if (!m_from || !m_to)
		return;
	std::complex<Core::f64> vFromKv =
		m_from->getVoltage() * m_from->getBaseVoltage();
	std::complex<Core::f64> vToKv = m_to->getVoltage() * m_to->getBaseVoltage();
	m_currentFlow = (vFromKv - vToKv) * 1000.0 * m_admittance;
}

std::string Line::toString() const {
	std::stringstream ss;
	ss << "[Line #" << m_id << "] " << m_name << " | "
	   << (m_from ? m_from->getName() : "None") << " -> "
	   << (m_to ? m_to->getName() : "None") << " | I: " << std::fixed
	   << std::setprecision(2) << std::abs(m_currentFlow) * 1000.0 << " A";
	return ss.str();
}

} // namespace GLStation::Grid

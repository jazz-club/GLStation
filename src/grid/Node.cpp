#include "grid/Node.hpp"
#include <iomanip>
#include <sstream>

/*
		potentially import voltage math to inidividual nodes?
		refer to sim/PowerSolver atm
*/
namespace GLStation::Grid {
/*
		normalise displayed voltage to modulus, |m_voltage| * m_baseVoltage
*/
Node::Node(std::string name, Core::f64 baseVoltage)
	: GridComponent(std::move(name)), m_baseVoltage(baseVoltage),
	  m_voltage(1.0, 0.0), m_energised(true) {}

void Node::tick(Core::Tick) {}

/*
		handler for both simple and vector voltage
		or now enitrely just for display, i do not understand where else we'd need that for maths
*/
std::string Node::toString() const {
	std::stringstream ss;
	ss << "[Node #" << m_id << "] " << m_name << " | " << std::fixed
	   << std::setprecision(3) << std::abs(m_voltage) << " pu ("
	   << std::abs(m_voltage) * m_baseVoltage << " kV)";
	return ss.str();
}

void Node::setVoltage(std::complex<Core::f64> voltage) { m_voltage = voltage; }

} // namespace GLStation::Grid

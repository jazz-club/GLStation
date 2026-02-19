#include "grid/Node.hpp"
#include <iomanip>
#include <sstream>

namespace GLStation::Grid {

Node::Node(std::string name, Core::f64 baseVoltage)
    : GridComponent(std::move(name)), m_baseVoltage(baseVoltage),
      m_voltage(1.0, 0.0) {}

void Node::tick(Core::Tick) {}

std::string Node::toString() const {
  std::stringstream ss;
  ss << "[Node #" << m_id << "] " << m_name << " | " << std::fixed
     << std::setprecision(3) << std::abs(m_voltage) << " pu ("
     << std::abs(m_voltage) * m_baseVoltage << " kV)";
  return ss.str();
}

void Node::setVoltage(std::complex<Core::f64> voltage) { m_voltage = voltage; }

} // namespace GLStation::Grid

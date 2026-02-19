#include "grid/Generator.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace GLStation::Grid {

Generator::Generator(std::string name, Node *connectedNode, GeneratorMode mode)
    : GridComponent(std::move(name)), m_connectedNode(connectedNode),
      m_mode(mode), m_targetP(0.0), m_actualP(0.0), m_targetV(1.0),
      m_minQ(-9999.0), m_maxQ(9999.0), m_droop(20.0), m_inertia(5.0),
      m_maxRampRate(500.0) {}

void Generator::applyDroopResponse(Core::f64 freqHz) {
  if (m_mode == GeneratorMode::Slack)
    return;
  Core::f64 df = freqHz - 50.0;
  Core::f64 dP = -m_droop * 1000.0 * df;
  Core::f64 desired = m_targetP + dP;
  Core::f64 maxDelta = m_maxRampRate;
  Core::f64 rawDelta = desired - m_actualP;
  Core::f64 clampedDelta = std::clamp(rawDelta, -maxDelta, maxDelta);
  m_actualP = std::clamp(m_actualP + clampedDelta, 0.0, m_targetP * 1.2);
}

void Generator::tick(Core::Tick) {
  m_actualP += (m_targetP - m_actualP) * 0.05;
}

std::string Generator::toString() const {
  std::string modeStr = "PQ";
  if (m_mode == GeneratorMode::Slack)
    modeStr = "Slack";
  else if (m_mode == GeneratorMode::PV)
    modeStr = "PV";
  std::stringstream ss;
  ss << "[Generator #" << m_id << "] " << m_name << " (" << modeStr
     << ") connected to: "
     << (m_connectedNode ? m_connectedNode->getName() : "NONE")
     << " | P=" << std::fixed << std::setprecision(1) << m_actualP << "kW"
     << ", V=" << std::fixed << std::setprecision(2) << m_targetV << "pu";
  return ss.str();
}

} // namespace GLStation::Grid

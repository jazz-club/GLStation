#include "grid/Load.hpp"
#include "util/Random.hpp"
#include <cmath>
#include <iomanip>
#include <sstream>

namespace GLStation::Grid {

Load::Load(std::string name, Node *connectedNode, Core::f64 maxPowerKw)
    : GridComponent(std::move(name)), m_connectedNode(connectedNode),
      m_maxPowerKw(maxPowerKw), m_currentPowerKw(0.0), m_powerFactor(0.95),
      m_isShed(false) {}

void Load::tick(Core::Tick currentTick) {
  if (m_isShed) {
    m_currentPowerKw = 0.0;
    return;
  }
  Core::u64 simHour = (currentTick / 150000) % 24;
  Core::f64 profile = 0.75;
  if (simHour < 6)
    profile = 0.55;
  else if (simHour < 9)
    profile = 0.55 + (simHour - 6) * 0.13;
  else if (simHour < 20)
    profile = 0.94 + Util::Random::range(-0.03, 0.04);
  else if (simHour < 23)
    profile = 0.94 - (simHour - 20) * 0.13;
  else
    profile = 0.55;
  Core::f64 noise = Util::Random::range(-0.01, 0.01);
  m_currentPowerKw = m_maxPowerKw * (profile + noise);
  if (m_currentPowerKw < 0.0)
    m_currentPowerKw = 0.0;
}

std::string Load::toString() const {
  std::stringstream ss;
  ss << "[Load #" << m_id << "] " << m_name << " | " << std::fixed
     << std::setprecision(2) << m_currentPowerKw << " kW";
  if (m_isShed)
    ss << " [SHED]";
  ss << " | Connected to: "
     << (m_connectedNode ? m_connectedNode->getName() : "None");
  return ss.str();
}

} // namespace GLStation::Grid

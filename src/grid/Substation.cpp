#include "grid/Substation.hpp"
#include <sstream>

namespace GLStation::Grid {

Substation::Substation(std::string name) : GridComponent(std::move(name)) {}

void Substation::addComponent(std::shared_ptr<GridComponent> component) {
  m_components.push_back(component);
}

void Substation::tick(Core::Tick currentTick) {
  for (auto &comp : m_components) {
    comp->tick(currentTick);
  }
}

std::string Substation::toString() const {
  std::stringstream ss;
  ss << "[Substation #" << m_id << "] " << m_name << " containing "
     << m_components.size() << " components:";
  for (const auto &comp : m_components) {
    ss << "\n  " << comp->toString();
  }
  return ss.str();
}

} // namespace GLStation::Grid

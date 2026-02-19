#include "grid/GridComponent.hpp"
#include <format>

namespace GLStation::Grid {

Core::u64 GridComponent::s_nextId = 1;

GridComponent::GridComponent(std::string name)
    : m_id(s_nextId++), m_name(std::move(name)) {}

std::string GridComponent::toString() const {
  return std::format("[Component #{}] {}", m_id, m_name);
}

} // namespace GLStation::Grid

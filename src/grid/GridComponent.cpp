#include "grid/GridComponent.hpp"
#include <format>

/*
		boilerplate class for all components
		useful for unifying inspect lookups by id
		for now this is just an iterative list of components with their data
		pushed to the context
*/
namespace GLStation::Grid {

Core::u64 GridComponent::s_nextId = 1;

GridComponent::GridComponent(std::string name)
	: m_id(s_nextId++), m_name(std::move(name)) {}

void GridComponent::resetIdCounter() { s_nextId = 1; }

std::string GridComponent::toString() const {
	return std::format("[Component #{}] {}", m_id, m_name);
}

} // namespace GLStation::Grid

#include "grid/Substation.hpp"
#include <sstream>

/*
		simpliest possible implementation, simply lets other grid elements inherit tick logic and acts as event handler 
*/
namespace GLStation::Grid {

Substation::Substation(std::string name) : GridComponent(std::move(name)) {}

/*
		potentially need to do better than this in the future since this makes cross referencing really hard
*/
void Substation::addComponent(std::shared_ptr<GridComponent> component) {
	m_components.push_back(component);
}

/*
    	forwards tick to subservient  components
*/
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

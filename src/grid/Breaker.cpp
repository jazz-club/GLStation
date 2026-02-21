#include "grid/Breaker.hpp"
#include <sstream>

/*
        Simple on/off, mostly unimplemented but m_from and m_to define high high
        admittance in YBus, probably not the best implementation, but thats
        essentially the abstraction of a breaker
*/

namespace GLStation::Grid {

Breaker::Breaker(std::string name, Node *from, Node *to)
	: GridComponent(std::move(name)), m_from(from), m_to(to), m_isOpen(false) {}

void Breaker::tick(Core::Tick) {}

std::string Breaker::toString() const {
	std::stringstream ss;
	ss << "[Breaker #" << m_id << "] " << m_name << " | "
	   << (m_from ? m_from->getName() : "N/A") << " <-> "
	   << (m_to ? m_to->getName() : "N/A")
	   << " | State: " << (m_isOpen ? "OPEN" : "CLOSED");
	return ss.str();
}

} // namespace GLStation::Grid

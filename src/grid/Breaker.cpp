#include "grid/Breaker.hpp"
#include "sim/PowerSolver.hpp"
#include "ui/Theme.hpp"
#include <sstream>

/*
        Simple on/off, mostly unimplemented but m_from and m_to define high high
        admittance in YBus, probably not the best implementation, but thats
        essentially the abstraction of a breaker
*/

namespace GLStation::Grid {

Breaker::Breaker(std::string name, Node *from, Node *to)
	: GridComponent(std::move(name)), m_from(from), m_to(to), m_isOpen(false) {}

void Breaker::setOpen(bool open) {
	m_isOpen = open;
	GLStation::Simulation::PowerSolver::invalidateYBus();
}

void Breaker::tick(Core::Tick) {}

std::string Breaker::toString() const {
	std::stringstream ss;
	ss << UI::Theme::cyan() << "[Breaker #" << m_id << "] "
	   << UI::Theme::reset() << m_name << UI::Theme::dim() << " | "
	   << (m_from ? m_from->getName() : "N/A") << " <-> "
	   << (m_to ? m_to->getName() : "N/A") << UI::Theme::reset() << " | State: "
	   << (m_isOpen ? UI::Theme::red() + "OPEN" : UI::Theme::green() + "CLOSED")
	   << UI::Theme::reset();
	return ss.str();
}

} // namespace GLStation::Grid

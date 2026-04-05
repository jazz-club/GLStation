#include "grid/Shunt.hpp"
#include "sim/PowerSolver.hpp"
#include <iomanip>
#include <sstream>

namespace GLStation::Grid {

Shunt::Shunt(std::string name, Node *node, Core::f64 gPu, Core::f64 bPu)
	: GridComponent(std::move(name)), m_node(node), m_gPu(gPu), m_bPu(bPu) {}

void Shunt::tick(Core::Tick) {}

void Shunt::setAdmittancePu(Core::f64 gPu, Core::f64 bPu) {
	m_gPu = gPu;
	m_bPu = bPu;
	GLStation::Simulation::PowerSolver::invalidateYBus();
}

std::string Shunt::toString() const {
	std::stringstream ss;
	ss << "[Shunt #" << m_id << "] " << m_name << " | G=" << std::fixed
	   << std::setprecision(4) << m_gPu << "pu B=" << m_bPu << "pu -> "
	   << (m_node ? m_node->getName() : "?");
	return ss.str();
}

} // namespace GLStation::Grid

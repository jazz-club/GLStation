#include "grid/Line.hpp"
#include "sim/PowerSolver.hpp"
#include <cmath>
#include <iomanip>
#include <sstream>

namespace GLStation::Grid {
/*
    	receiver pathing, I = Y ΔV; losses P_Loss = |I|^2 R
    	useful for admittance matrix population in the power solver
    	since its lower level at line
    	Z=r + jx while Y = 1/Z as long as |Z| ≤ ε, prevents impedance rolling over
		to division by 0, forces impedance to 0 when needed current amp limit is set
		to 500amps, fix when get data
*/
Line::Line(std::string name, Node *from, Node *to, Core::f64 r, Core::f64 x,
		   Core::f64 lineChargingSusceptancePu)
	: GridComponent(std::move(name)), m_from(from), m_to(to), m_resistance(r),
	  m_reactance(x), m_lineChargingSusceptancePu(lineChargingSusceptancePu),
	  m_currentLimit(500.0), m_seriesFlowPu(0, 0), m_distanceRelay(false),
	  m_distanceReachZohm(0.0) {
	std::complex<Core::f64> impedance(r, x);
	if (std::abs(impedance) > Core::EPSILON) {
		m_admittance = 1.0 / impedance;
	} else {
		m_admittance = 0.0;
	}
}

void Line::setResistanceReactance(Core::f64 r, Core::f64 x) {
	m_resistance = r;
	m_reactance = x;
	std::complex<Core::f64> impedance(r, x);
	if (std::abs(impedance) > Core::EPSILON) {
		m_admittance = 1.0 / impedance;
	} else {
		m_admittance = 0.0;
	}
	GLStation::Simulation::PowerSolver::invalidateYBus();
}

void Line::setLineChargingSusceptancePu(Core::f64 bTotalPu) {
	m_lineChargingSusceptancePu = bTotalPu;
	GLStation::Simulation::PowerSolver::invalidateYBus();
}

void Line::setFlowFromSolverPu(std::complex<Core::f64> seriesIpu,
							   Core::f64 rPuEff) {
	(void)rPuEff;
	m_seriesFlowPu = seriesIpu;
	if (!m_from || !m_to)
		return;
	Core::f64 vllKv = m_from->getBaseVoltage();
	Core::f64 sbaseMva = GLStation::Simulation::PowerSolver::sBaseMva();
	Core::f64 sMva = std::abs(seriesIpu) * sbaseMva;
	if (vllKv > 1e-9)
		m_currentFlow = sMva * 1000.0 / (std::sqrt(3.0) * vllKv);
	else
		m_currentFlow = 0.0;
}

void Line::setDistanceRelay(Core::f64 reachZohm, bool enabled) {
	m_distanceReachZohm = reachZohm;
	m_distanceRelay = enabled;
}

/*
    	tick math, I = Y × (m_fromV − m_toV), I = ΔV x 1000 x Y, (kV is more human readable)
		so conversion and returns current in amps
*/
void Line::tick(Core::Tick) {
	if (!m_from || !m_to)
		return;
	if (std::abs(m_seriesFlowPu) > 1e-15)
		return;
	std::complex<Core::f64> vFromKv =
		m_from->getVoltage() * m_from->getBaseVoltage() / std::sqrt(3.0);
	std::complex<Core::f64> vToKv =
		m_to->getVoltage() * m_to->getBaseVoltage() / std::sqrt(3.0);
	m_currentFlow = (vFromKv - vToKv) * 1000.0 * m_admittance;
}

std::string Line::toString() const {
	std::stringstream ss;
	ss << "[Line #" << m_id << "] " << m_name << " | "
	   << (m_from ? m_from->getName() : "None") << " -> "
	   << (m_to ? m_to->getName() : "None") << " | I: " << std::fixed
	   << std::setprecision(2) << std::abs(m_currentFlow) << " A";
	return ss.str();
}

} // namespace GLStation::Grid

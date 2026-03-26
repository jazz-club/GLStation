#include "grid/Generator.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>

/*
        Basic AGC and load balancing droop, ΔP ∝ −(f − 50),
        power output is inversely proportional to deviation in frequency from
		standard
*/

namespace GLStation::Grid {

Generator::Generator(std::string name, Node *connectedNode, GeneratorMode mode)
	: GridComponent(std::move(name)), m_connectedNode(connectedNode),
	  m_mode(mode), m_targetP(0.0), m_actualP(0.0), m_targetV(1.0),
	  m_minQ(-9999.0), m_maxQ(9999.0), m_droop(20.0), m_inertia(5.0),
	  m_maxRampRate(500.0) {}

/*
        default frequency set to 50 for now, i dont know enough about
        electricity to get into the countries that use different standard
        frequencies dP defines loading balancing droop when deviation from
        frequency from 50 desired defines target relative to deviation
        clampedDelta forces target max deviation capped at 1.1, ramp limiting
		the generator ISO marks 1.1 as the ideal overload but if we should limit this
		i have no idea potentially uncap this in the future just so we're more
        realistic i guess https://www.evs.ee/en/iso-3046-1-1995
*/
void Generator::applyDroopResponse(Core::f64 freqHz) {
	if (m_mode == GeneratorMode::Slack)
		return;
	Core::f64 df = freqHz - 50.0;
	Core::f64 dP = -m_droop * 1000.0 * df;
	Core::f64 desired = m_targetP + dP;
	Core::f64 maxDelta = m_maxRampRate;
	Core::f64 rawDelta = desired - m_actualP;
	Core::f64 clampedDelta = std::clamp(rawDelta, -maxDelta, maxDelta);
	m_actualP = std::clamp(m_actualP + clampedDelta, 0.0, m_targetP * 1.1);
}

/*
        droop rate for moving single phase generation towards target deramp
        1% per tick at the moment, not sure what a realistic value would
   		actually be need to implement 3 phase generation at some point
*/
void Generator::tick(Core::Tick) {
	if (m_mode == GeneratorMode::Slack)
		return;
	m_actualP += (m_targetP - m_actualP) * 0.01;
}

/*
        converting all the variables above into a human readable format
        slightly too hard coded for my liking at the moment
        maybe implement internal unified schema for all components at some
   		point?
*/
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

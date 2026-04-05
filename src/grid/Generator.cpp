#include "grid/Generator.hpp"
#include "util/Random.hpp"
#include <algorithm>
#include <cmath>
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
	  m_mode(mode), m_profile(GeneratorProfile::Manual), m_targetP(0.0),
	  m_actualP(0.0), m_targetV(1.0), m_minQ(-9999.0), m_maxQ(9999.0),
	  m_minP(0.0), m_maxP(200000.0), m_droop(20.0), m_inertia(5.0),
	  m_maxRampRate(500.0), m_profileStrength(1.0), m_droopTargetKw(0.0),
	  m_exciterVpu(1.0), m_actualQKw(0.0), m_pvQClamp(false), m_qFixedKw(0.0),
	  m_windAr(0.0) {
	m_droopTargetKw = m_actualP;
}

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
void Generator::applyDroopResponse(Core::f64 freqHz, Core::f64 nominalHz) {
	if (m_mode == GeneratorMode::Slack)
		return;
	Core::f64 df = freqHz - nominalHz;
	Core::f64 dP = -m_droop * 1000.0 * df;
	m_droopTargetKw = std::clamp(m_targetP + dP, m_minP, m_maxP);
}

void Generator::stepGovernor(Core::f64 dtSec) {
	(void)dtSec;
	if (m_mode == GeneratorMode::Slack)
		return;
	Core::f64 alpha = 0.12;
	Core::f64 rawDelta = m_droopTargetKw - m_actualP;
	Core::f64 step =
		std::clamp(alpha * rawDelta, -m_maxRampRate, m_maxRampRate);
	m_actualP = std::clamp(m_actualP + step, m_minP, m_maxP);
}

void Generator::stepExciter(Core::f64 dtSec) {
	(void)dtSec;
	if (m_mode == GeneratorMode::Slack) {
		m_exciterVpu = m_targetV;
		return;
	}
	Core::f64 beta = 0.08;
	m_exciterVpu += beta * (m_targetV - m_exciterVpu);
}

/*
        droop rate for moving single phase generation towards target deramp
        1% per tick at the moment, not sure what a realistic value would
   		actually be need to implement 3 phase generation at some point
*/
void Generator::tick(Core::Tick) {
	if (m_mode == GeneratorMode::Slack)
		return;
	/*
	refer to Generator.hpp
*/
	Core::f64 profileScale = 1.0;
	if (m_profile == GeneratorProfile::Wind) {
		Core::f64 phi = 0.92;
		Core::f64 noise = Util::Random::range(-0.08, 0.08) * m_profileStrength;
		m_windAr = phi * m_windAr + noise;
		m_windAr = std::clamp(m_windAr, -0.35, 0.35);
		profileScale = std::clamp(
			0.70 + m_windAr + Util::Random::range(-0.05, 0.05), 0.2, 1.25);
	} else if (m_profile == GeneratorProfile::Solar) {
		profileScale =
			0.78 + Util::Random::range(-0.20, 0.20) * m_profileStrength;
	} else if (m_profile == GeneratorProfile::Hydro)
		profileScale =
			0.92 + Util::Random::range(-0.05, 0.06) * m_profileStrength;
	else if (m_profile == GeneratorProfile::Thermal)
		profileScale =
			1.00 + Util::Random::range(-0.02, 0.03) * m_profileStrength;
	profileScale = std::clamp(profileScale, 0.2, 1.25);

	Core::f64 profiledTarget =
		std::clamp(m_targetP * profileScale, m_minP, m_maxP);
	Core::f64 rawDelta = profiledTarget - m_actualP;
	Core::f64 clampedDelta =
		std::clamp(rawDelta, -m_maxRampRate, m_maxRampRate);
	m_actualP = std::clamp(m_actualP + clampedDelta, m_minP, m_maxP);
	m_droopTargetKw = m_actualP;
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

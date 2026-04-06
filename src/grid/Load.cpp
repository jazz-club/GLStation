#include "grid/Load.hpp"
#include "sim/Engine.hpp"
#include "util/Random.hpp"
#include <cmath>
#include <iomanip>
#include <sstream>

/*
		shedding is currently mostly unutilised, if the current phase load is at max - noise, shed
		load is calculated at each individual node for more freedom with calculating scaling at a higher level
*/
namespace GLStation::Grid {

/*
		refer to https://repository.tilburguniversity.edu/server/api/core/bitstreams/dd61c107-5eb3-4b21-97d7-88d7b86ed0b2/content
		solve as Q = P √(1−pf^2)/pf in solver
*/
Load::Load(std::string name, Node *connectedNode, Core::f64 maxPowerKw)
	: GridComponent(std::move(name)), m_connectedNode(connectedNode),
	  m_maxPowerKw(maxPowerKw), m_currentPowerKw(0.0), m_powerFactor(0.95),
	  m_isShed(false), m_profile(LoadProfile::Flat), m_profileStrength(1.0),
	  m_freqSens(-0.02), m_zipZp(1.0), m_profileScale(0.80) {}

/*
		hourly load shedding, profiling, and noise filtering
		entirely useless at the moment since we havent figured out how to define variable loads other than generic residential slash industrial slash whatever loads
		case switching is self explanatory but i dont think having set definitions for profile segments is a good idea long term
*/
void Load::tick(Core::Tick currentTick) {
	if (m_isShed) {
		m_currentPowerKw = 0.0;
		return;
	}
	if (m_connectedNode && !m_connectedNode->isEnergised()) {
		m_currentPowerKw = 0.0;
		return;
	}
	auto &st = GLStation::Simulation::Engine::simTickState();

	if (currentTick % 1000 == 0 || currentTick <= 1) {
		Core::u64 ms = static_cast<Core::u64>(st.simTime.count());
		Core::u64 simHour = (ms / 3600000ULL) % 24ULL;
		/*
		TODO add more sim profiles for other load types
	*/
		Core::f64 profile = 0.80;

		if (m_profile == LoadProfile::Residential) {
			if (simHour < 5)
				profile = 0.45;
			else if (simHour < 8)
				profile = 0.55 + static_cast<Core::f64>(simHour - 5) * 0.15;
			else if (simHour < 16)
				profile = 0.72;
			else if (simHour < 21)
				profile = 0.82 + static_cast<Core::f64>(simHour - 16) * 0.06;
			else
				profile = 1.00 - static_cast<Core::f64>(simHour - 21) * 0.12;
		} else if (m_profile == LoadProfile::Commercial) {
			if (simHour < 6)
				profile = 0.30;
			else if (simHour < 9)
				profile = 0.50 + static_cast<Core::f64>(simHour - 6) * 0.16;
			else if (simHour < 18)
				profile = 0.95;
			else if (simHour < 21)
				profile = 0.82 - static_cast<Core::f64>(simHour - 18) * 0.18;
			else
				profile = 0.30;
		} else if (m_profile == LoadProfile::Industrial) {
			if (simHour < 6)
				profile = 0.70;
			else if (simHour < 18)
				profile = 0.92;
			else
				profile = 0.80;
		}

		Core::f64 weatherJitter =
			Util::Random::range(-0.05, 0.05) * m_profileStrength;
		Core::f64 noise = Util::Random::range(-0.02, 0.02);
		m_profileScale = (profile + noise) * (1.0 + weatherJitter);
		if (m_profileScale < 0.0)
			m_profileScale = 0.0;
	}

	m_currentPowerKw = m_maxPowerKw * m_profileScale;
	if (m_currentPowerKw < 0.0)
		m_currentPowerKw = 0.0;
	Core::f64 fn = st.nominalHz;
	if (fn > 1e-6) {
		m_currentPowerKw *= (1.0 + m_freqSens * (st.systemHz - fn) / fn);
	}
	if (m_connectedNode) {
		Core::f64 vm = std::abs(m_connectedNode->getVoltage());
		m_currentPowerKw *= (m_zipZp * vm + (1.0 - m_zipZp) * vm * vm);
	}
	if (m_currentPowerKw < 0.0)
		m_currentPowerKw = 0.0;
}

std::string Load::toString() const {
	std::stringstream ss;
	ss << "[Load #" << m_id << "] " << m_name << " | " << std::fixed
	   << std::setprecision(2) << m_currentPowerKw << " kW";
	if (m_isShed)
		ss << " [SHED]";
	ss << " | Connected to: "
	   << (m_connectedNode ? m_connectedNode->getName() : "None");
	return ss.str();
}

} // namespace GLStation::Grid

#include "grid/Breaker.hpp"
#include "grid/Line.hpp"
#include "grid/Node.hpp"
#include "sim/Engine.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>

namespace GLStation::Simulation {

/*
		redundant until scenario manager gets somewhere but breaker logic
*/
void Engine::processProtectionRelays() {
	m_maxObservedLineLoadingPercent = 0.0;
	std::map<Core::u64, Grid::Breaker *> breakerById;
	std::map<std::string, Grid::Breaker *> breakerByEdge;
	auto edgeKey = [](Grid::Node *a, Grid::Node *b) -> std::string {
		Core::u64 idA = a ? a->getId() : 0;
		Core::u64 idB = b ? b->getId() : 0;
		if (idA > idB)
			std::swap(idA, idB);
		return std::to_string(idA) + "-" + std::to_string(idB);
	};

	for (auto &sub : m_substations) {
		for (auto &comp : sub->getComponents()) {
			if (auto brk = dynamic_cast<Grid::Breaker *>(comp.get())) {
				breakerById[brk->getId()] = brk;
				breakerByEdge[edgeKey(brk->getFromNode(), brk->getToNode())] =
					brk;
			}
		}
	}

	for (auto &[id, recloseTick] : m_recloseAtTick) {
		if (recloseTick <= m_currentTick && breakerById.contains(id)) {
			if (m_recloseCooldownUntil.count(id) &&
				m_currentTick < m_recloseCooldownUntil[id])
				continue;
			if (m_recloseLockout.count(id) && m_recloseLockout[id] >= 3)
				continue;
			Grid::Breaker *brk = breakerById[id];
			if (brk->isOpen()) {
				brk->setOpen(false);
				m_recloseCooldownUntil[id] = m_currentTick + 800;
				m_recloseLockout[id]++;
				logEvent("[RELAY] Auto-reclosed breaker " + brk->getName(),
						 Log::Severity::Critical);
			}
		}
	}
	for (auto it = m_recloseAtTick.begin(); it != m_recloseAtTick.end();) {
		if (it->second <= m_currentTick)
			it = m_recloseAtTick.erase(it);
		else
			++it;
	}

	for (auto &sub : m_substations) {
		for (auto &comp : sub->getComponents()) {
			if (auto line = dynamic_cast<Grid::Line *>(comp.get())) {
				Core::f64 iMag = std::abs(line->getCurrentFlow());
				Core::f64 lim =
					std::max<Core::f64>(1.0, line->getCurrentLimit());
				Core::f64 loadingPct = 100.0 * iMag / lim;
				m_maxObservedLineLoadingPercent =
					std::max(m_maxObservedLineLoadingPercent, loadingPct);

				std::string key =
					edgeKey(line->getFromNode(), line->getToNode());
				if (!breakerByEdge.contains(key))
					continue;
				Grid::Breaker *brk = breakerByEdge[key];

				Core::f64 pickup = 1.05 * lim;
				if (line->isDistanceRelayEnabled() && line->getFromNode() &&
					line->getToNode()) {
					Core::f64 vf = std::abs(line->getFromNode()->getVoltage()) *
								   line->getFromNode()->getBaseVoltage();
					Core::f64 vt = std::abs(line->getToNode()->getVoltage()) *
								   line->getToNode()->getBaseVoltage();
					Core::f64 zapp = std::abs(vf - vt) / std::max(1e-6, iMag);
					if (zapp < line->getDistanceReachZohm() &&
						iMag > 0.05 * lim) {
						m_pendingTrips[brk->getId()] = m_currentTick + 40;
					}
				}

				if (iMag > pickup) {
					if (!m_overloadStartTick.contains(brk->getId())) {
						m_overloadStartTick[brk->getId()] = m_currentTick;
						std::ostringstream os;
						os << "[RELAY] " << line->getName() << " overload ("
						   << std::fixed << std::setprecision(1) << loadingPct
						   << "%), trip timer started";
						logEvent(os.str(), Log::Severity::Critical);
					}
					Core::f64 ratio = iMag / lim;
					Core::f64 inv = 400.0 / std::max(ratio / 1.05 - 1.0, 0.08);
					m_pendingTrips[brk->getId()] =
						m_overloadStartTick[brk->getId()] +
						static_cast<Core::u64>(inv);
				} else {
					m_pendingTrips.erase(brk->getId());
					m_overloadStartTick.erase(brk->getId());
				}
			}
		}
	}

	for (auto it = m_pendingTrips.begin(); it != m_pendingTrips.end();) {
		Core::u64 breakerId = it->first;
		Core::u64 tripAt = it->second;
		if (tripAt <= m_currentTick && breakerById.contains(breakerId)) {
			Grid::Breaker *brk = breakerById[breakerId];
			if (!brk->isOpen()) {
				brk->setOpen(true);
				if (m_recloseLockout[breakerId] < 3) {
					m_recloseAtTick[breakerId] = m_currentTick + 1500;
					logEvent("[RELAY] Tripped " + brk->getName() +
								 ", auto-reclose armed",
							 Log::Severity::Critical);
				} else {
					logEvent("[RELAY] Tripped " + brk->getName() +
								 ", locked out after 3 reclose attempts",
							 Log::Severity::Critical);
				}
			}
			it = m_pendingTrips.erase(it);
		} else {
			++it;
		}
	}

	processUFLS();
}

} // namespace GLStation::Simulation

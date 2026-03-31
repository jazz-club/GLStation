#include "sim/ScenarioManager.hpp"
#include <algorithm>

/*
		name is slightly misleading at the moment, current just an event scheduler for breakers
		massive TODO is to implement stellaris crisis style scenarios
*/
namespace GLStation::Simulation {

void ScenarioManager::addEvent(Core::u64 tick, EventAction action) {
	m_events[tick].push_back(action);
}

void ScenarioManager::update(Core::u64 currentTick) {
	if (m_events.contains(currentTick)) {
		for (auto &action : m_events[currentTick]) {
			action();
		}
		m_events.erase(currentTick);
	}
}

std::vector<Core::u64> ScenarioManager::getScheduledTicks() const {
	std::vector<Core::u64> out;
	for (const auto &kv : m_events)
		out.push_back(kv.first);
	std::sort(out.begin(), out.end());
	return out;
}

void ScenarioManager::clear() { m_events.clear(); }

size_t ScenarioManager::getScheduledEventCount() const {
	size_t total = 0;
	for (const auto &kv : m_events)
		total += kv.second.size();
	return total;
}

} // namespace GLStation::Simulation

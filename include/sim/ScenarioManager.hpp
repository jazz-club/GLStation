#pragma once

#include "util/Types.hpp"
#include <cstddef>
#include <functional>
#include <map>
#include <vector>

namespace GLStation::Simulation {

class ScenarioManager {
  public:
	using EventAction = std::function<void()>;

	void addEvent(Core::u64 tick, EventAction action);
	void update(Core::u64 currentTick);
	std::vector<Core::u64> getScheduledTicks() const;
	void clear();
	size_t getScheduledEventCount() const;

  private:
	std::map<Core::u64, std::vector<EventAction>> m_events;
};

} // namespace GLStation::Simulation

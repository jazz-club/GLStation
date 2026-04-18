#pragma once

#include "util/Types.hpp"

namespace GLStation::Simulation {
class Engine;
}

namespace GLStation::UI {

static const Core::u64 RUN_DASH_INTERVAL_TICKS = 1000;

void printLiveDashboard(const Simulation::Engine &engine, bool moveUp,
						bool showProgress, Core::u64 doneTicks,
						Core::u64 totalTicks);

} // namespace GLStation::UI

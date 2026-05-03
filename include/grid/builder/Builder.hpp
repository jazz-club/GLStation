#pragma once

#include <memory>

namespace GLStation::Simulation {
class Engine;
}

namespace GLStation::Grid {
class Substation;
}

namespace GLStation::Grid::Builder {

class Builder {
  public:
	static void runLoop(Simulation::Engine &engine);
	static std::shared_ptr<Substation> getActiveSubstation();
	static void setActiveSubstation(std::shared_ptr<Substation> sub);
};

} // namespace GLStation::Grid::Builder

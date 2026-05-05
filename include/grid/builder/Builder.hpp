#pragma once

#include <memory>

namespace GLStation::Simulation {
class Engine;
}

namespace GLStation::Grid {
class Substation;
}

namespace GLStation::Grid::Builder {

class BuilderShell {
  public:
	static std::shared_ptr<Substation> getActiveSubstation();
	static void setActiveSubstation(std::shared_ptr<Substation> sub);
};

} // namespace GLStation::Grid::Builder

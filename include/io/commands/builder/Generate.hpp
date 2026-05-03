#pragma once

#include <sstream>
#include <string>

namespace GLStation::Simulation {
class Engine;
}

namespace GLStation::IO::Commands::Builder {

class Generate {
  public:
	static void execute(Simulation::Engine &engine, std::stringstream &ss);
};

} // namespace GLStation::IO::Commands::Builder

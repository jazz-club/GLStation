#pragma once
#include <string>
#include <vector>

namespace GLStation::Simulation {
class Engine;
}

namespace GLStation::IO::Commands::Builder {
void cmdAdd(Simulation::Engine &engine, const std::vector<std::string> &args);
void cmdSave(Simulation::Engine &engine, const std::vector<std::string> &args);
void cmdLoad(Simulation::Engine &engine, const std::vector<std::string> &args);
void cmdGenerate(Simulation::Engine &engine,
				 const std::vector<std::string> &args);
void cmdValidate(Simulation::Engine &engine,
				 const std::vector<std::string> &args);
void cmdUse(Simulation::Engine &engine, const std::vector<std::string> &args);
void cmdClear(Simulation::Engine &engine, const std::vector<std::string> &args);
} // namespace GLStation::IO::Commands::Builder

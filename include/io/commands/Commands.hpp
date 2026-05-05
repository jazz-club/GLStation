#pragma once
#include <string>
#include <vector>

namespace GLStation::Simulation {
class Engine;
}

namespace GLStation::IO::Commands {
void cmdRun(Simulation::Engine &engine, const std::vector<std::string> &args);
void cmdStatus(Simulation::Engine &engine,
			   const std::vector<std::string> &args);
void cmdFind(Simulation::Engine &engine, const std::vector<std::string> &args);
void cmdTree(Simulation::Engine &engine, const std::vector<std::string> &args);
void cmdList(Simulation::Engine &engine, const std::vector<std::string> &args);
void cmdInspect(Simulation::Engine &engine,
				const std::vector<std::string> &args);
void cmdSet(Simulation::Engine &engine, const std::vector<std::string> &args);
void cmdOpen(Simulation::Engine &engine, const std::vector<std::string> &args);
void cmdClose(Simulation::Engine &engine, const std::vector<std::string> &args);
void cmdExport(Simulation::Engine &engine,
			   const std::vector<std::string> &args);
} // namespace GLStation::IO::Commands

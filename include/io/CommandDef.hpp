#pragma once

#include <functional>
#include <string>
#include <vector>

namespace GLStation::Simulation {
class Engine;
}

namespace GLStation::IO {

struct CommandDef {
	std::string name;
	std::string usage;
	std::string brief;
	std::function<void(Simulation::Engine &, const std::vector<std::string> &)>
		handler;
};

const std::vector<CommandDef> &getMainCommands();
const std::vector<CommandDef> &getBuilderCommands();
void printHelp(const std::vector<CommandDef> &commands);

} // namespace GLStation::IO

#include "io/CommandDef.hpp"
#include "io/commands/BuilderCommands.hpp"
#include "io/commands/Commands.hpp"
#include "ui/Theme.hpp"
#include <iostream>

namespace GLStation::IO {

const std::vector<CommandDef> &getMainCommands() {
	static const std::vector<CommandDef> table = {
		{"run", "<time> | inf", "Advance the simulation", Commands::cmdRun},
		{"status", "", "Show system overview", Commands::cmdStatus},
		{"find", "<query>", "Search components by name", Commands::cmdFind},
		{"tree", "", "Display grid topology", Commands::cmdTree},
		{"list", "<gen|load|line|trafo|breaker>", "List components by type",
		 Commands::cmdList},
		{"inspect", "<id>", "Inspect component details", Commands::cmdInspect},
		{"set", "<id> <param> <value>", "Modify component parameter",
		 Commands::cmdSet},
		{"open", "<id>", "Open a breaker", Commands::cmdOpen},
		{"close", "<id>", "Close a breaker", Commands::cmdClose},
		{"export", "[filename]", "Export voltages to CSV", Commands::cmdExport},
	};
	return table;
}

const std::vector<CommandDef> &getBuilderCommands() {
	static const std::vector<CommandDef> table = {
		{"add", "<type> <args...>", "Add a grid component",
		 Commands::Builder::cmdAdd},
		{"use", "<substation_name>", "Switch active substation",
		 Commands::Builder::cmdUse},
		{"save", "<filename.csv>", "Save grid to CSV",
		 Commands::Builder::cmdSave},
		{"load", "<filename.csv>", "Load grid from CSV",
		 Commands::Builder::cmdLoad},
		{"generate", "grid <name> <population>", "Generate procedural grid",
		 Commands::Builder::cmdGenerate},
		{"validate", "", "Run grid validation checks",
		 Commands::Builder::cmdValidate},
		{"clear", "", "Clear all substations", Commands::Builder::cmdClear},
		{"tree", "", "Display grid topology", Commands::cmdTree},
		{"inspect", "<id>", "Inspect component details", Commands::cmdInspect},
	};
	return table;
}

void printHelp(const std::vector<CommandDef> &commands) {
	std::cout << "\n"
			  << UI::Theme::cyan() << "Commands:" << UI::Theme::reset()
			  << "\n\n";
	for (const auto &cmd : commands) {
		std::cout << UI::Theme::green() << "  " << cmd.name << " "
				  << UI::Theme::yellow() << cmd.usage << UI::Theme::reset();
		if (!cmd.brief.empty())
			std::cout << UI::Theme::dim() << "  - " << cmd.brief
					  << UI::Theme::reset();
		std::cout << "\n";
	}
	std::cout << UI::Theme::green() << "  exit" << UI::Theme::reset() << "\n"
			  << std::endl;
}

} // namespace GLStation::IO

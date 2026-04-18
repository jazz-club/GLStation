#include "io/commands/Help.hpp"
#include "ui/Terminal.hpp"
#include <iostream>

namespace GLStation::IO::Commands {

void Help::execute() {
	std::string cyn = UI::isAnsiEnabled() ? UI::ANSI_CYAN : "";
	std::string res = UI::isAnsiEnabled() ? UI::ANSI_RESET : "";
	std::string yel = UI::isAnsiEnabled() ? UI::ANSI_YELLOW : "";
	std::string grn = UI::isAnsiEnabled() ? UI::ANSI_GREEN : "";
	std::cout << "\n"
			  << cyn << "--- Commands ---" << res << "\n\n"
			  << grn << "  exit" << res << "\n"

			  << grn << "  run " << yel << "<time> [realtime]" << res << "\n"
			  << grn << "  run inf " << yel << "[realtime]" << res << "\n"
			  << grn << "  status" << res << "\n"
			  << grn << "  find " << yel << "<query>" << res << "\n"
			  << grn << "  tree" << res << "\n"
			  << grn << "  list " << yel << "<gen|load|line|trafo|breaker>"
			  << res << "\n"
			  << grn << "  inspect " << yel << "<id>" << res << "\n"
			  << grn << "  set " << yel << "<id> <param> <value>" << res << "\n"
			  << grn << "  open " << yel << "<id>" << res << "\n"
			  << grn << "  close " << yel << "<id>" << res << "\n"
			  << grn << "  export " << yel << "[filename]" << res << "\n"
			  << grn << "  import " << yel << "<name>" << res << "\n"
			  << grn << "  import demo" << res << "\n"
			  << std::endl;
}

} // namespace GLStation::IO::Commands

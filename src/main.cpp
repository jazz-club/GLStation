#include "io/handlers/CommandHandler.hpp"
#include "sim/Engine.hpp"
#include "ui/Terminal.hpp"
#include <iostream>

int main() {
	GLStation::UI::enableAnsiIfPossible();
	try {
		std::string cyn =
			GLStation::UI::isAnsiEnabled() ? GLStation::UI::ANSI_CYAN : "";
		std::string res =
			GLStation::UI::isAnsiEnabled() ? GLStation::UI::ANSI_RESET : "";

		std::cout << "\n"
				  << cyn << "   GLStation\n"
				  << res << "   Type 'help' for commands.\n\n";

		GLStation::Simulation::Engine engine;
		engine.initialise();

		GLStation::IO::runLoop(engine);

	} catch (const std::exception &e) {
		std::cerr << "FUUUCK: " << e.what() << std::endl;
	}
	/*
	^thought about changing this but never had a runtime exception so lets leave it as an easter egg
*/
	std::cout << "Exiting..." << std::endl;
	return 0;
}

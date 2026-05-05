#include "io/handlers/CommandHandler.hpp"
#include "sim/Engine.hpp"
#include "ui/Terminal.hpp"
#include "ui/Theme.hpp"
#include <iostream>

int main() {
	GLStation::UI::enableAnsiIfPossible();
	try {
		std::cout << "\n"
				  << GLStation::UI::Theme::cyan() << "   GLStation\n"
				  << GLStation::UI::Theme::reset()
				  << "   Type 'help' for commands.\n\n";

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

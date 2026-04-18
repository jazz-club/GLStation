#include "io/commands/Import.hpp"
#include "io/handlers/GridHandler.hpp"
#include "io/handlers/InputHandler.hpp"
#include "log/Logger.hpp"
#include "log/Result.hpp"
#include <filesystem>
#include <iostream>

namespace GLStation::IO::Commands {

void Import::execute(Simulation::Engine &engine,
					 const std::string &cityNameArg) {
	std::string cityName = Util::InputHandler::trim(cityNameArg);
	if (!cityName.empty()) {
		std::string cityNorm =
			Util::InputHandler::normaliseForComparison(cityName);
		if (cityNorm == "demo") {
			std::error_code ec;
			(void)std::filesystem::remove("grid.csv", ec);
			engine.initialise();
			std::cout << "Demo grid loaded." << std::endl;
			return;
		}

		auto st = Simulation::GridHandler::importCity(cityName);
		if (st.isSuccess()) {
			engine.initialise();
			Log::Logger::info("Grid loaded for: " + cityName);
		} else {
			if (st.code == Log::ErrorCode::NotFound) {
				auto suggestions =
					Simulation::GridHandler::getSuggestions(cityName);
				if (!suggestions.empty()) {
					std::cout << "\nNo exact match for \"" << cityName
							  << "\". Closest matches:" << std::endl;
					for (size_t i = 0; i < suggestions.size(); ++i)
						std::cout << "  " << (i + 1) << ". " << suggestions[i]
								  << std::endl;
					std::cout << "Pick (1-" << suggestions.size()
							  << ") or cancel: " << std::flush;
					std::string choice;
					if (std::getline(std::cin, choice)) {
						choice = Util::InputHandler::trim(choice);
						size_t idx = 0;
						try {
							idx = static_cast<size_t>(std::stoul(choice));
						} catch (...) {
						}
						if (idx >= 1 && idx <= suggestions.size()) {
							auto retrySt = Simulation::GridHandler::importCity(
								suggestions[idx - 1]);
							if (retrySt.isSuccess()) {
								engine.initialise();
								Log::Logger::info("Grid loaded for: " +
												  suggestions[idx - 1]);
							} else {
								Log::Logger::error(retrySt.message);
							}
						}
					}
				} else {
					Log::Logger::error(st.message);
				}
			} else {
				Log::Logger::error(st.message);
			}
		}
	} else {
		std::cout << "Usage: import <name> | import demo" << std::endl;
	}
}

} // namespace GLStation::IO::Commands

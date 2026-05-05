#include "io/commands/Commands.hpp"
#include "io/handlers/InputHandler.hpp"
#include "log/Logger.hpp"
#include "sim/Engine.hpp"
#include "ui/Dashboard.hpp"
#include "ui/Terminal.hpp"
#include "ui/Theme.hpp"
#include "util/CommandUtils.hpp"
#include <chrono>
#include <iostream>
#include <thread>

namespace GLStation::IO::Commands {

void cmdRun(Simulation::Engine &engine, const std::vector<std::string> &args) {
	if (args.empty()) {
		Log::Logger::warn("Usage: run <time> | run inf");
		return;
	}

	bool realtime = false;
	if (args.size() >= 2) {
		if (InputHandler::normaliseForComparison(args[1]) == "realtime") {
			realtime = true;
		} else {
			Log::Logger::warn("Usage: run <time> | run inf");
			return;
		}
	}

	std::string argNorm = InputHandler::normaliseForComparison(args[0]);
	UI::enableAnsiIfPossible();

	if (argNorm == "inf") {
		std::cout << "Run indefinitely. Press X to stop.\n" << std::endl;
		UI::ScopedRawStdin rawInput;
		(void)rawInput;
		bool first = true;
		auto limitTime = std::chrono::steady_clock::now();
		for (;;) {
			auto tStartTick = std::chrono::steady_clock::now();
			if (UI::shouldStopRun())
				goto done_inf;

			engine.tick();

			auto tNow = std::chrono::steady_clock::now();
			auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(
						   tNow - limitTime)
						   .count();

			if (dur >= 100) {
				UI::printLiveDashboard(engine, !first, true,
									   engine.getTickCount(), 0);
				first = false;
				limitTime = tNow;
			}

			if (realtime) {
				auto elapsedTick =
					std::chrono::duration_cast<std::chrono::milliseconds>(
						std::chrono::steady_clock::now() - tStartTick)
						.count();
				if (elapsedTick < 10) {
					std::this_thread::sleep_for(
						std::chrono::milliseconds(10 - elapsedTick));
				}
			}
		}
	done_inf:
		UI::printLiveDashboard(engine, !first, false, engine.getTickCount(), 0);
	} else {
		Core::u64 durationTicks = 0;
		if (!Util::CommandUtils::parseRunDurationTicks(args[0],
													   durationTicks)) {
			Log::Logger::warn("Usage: run <time> | run inf");
			return;
		}
		std::cout << "Running for " << durationTicks
				  << " ticks. Press X to stop.\n"
				  << std::endl;

		UI::ScopedRawStdin rawInput;
		(void)rawInput;
		bool first = true;
		Core::u64 done = 0;
		auto limitTime = std::chrono::steady_clock::now();
		for (; done < durationTicks;) {
			auto tStartTick = std::chrono::steady_clock::now();
			if (UI::shouldStopRun()) {
				done = durationTicks;
				break;
			}

			engine.tick();
			++done;

			auto tNow = std::chrono::steady_clock::now();
			auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(
						   tNow - limitTime)
						   .count();

			if (dur >= 100 || done == durationTicks) {
				UI::printLiveDashboard(engine, (done > 1 && !first), true, done,
									   durationTicks);
				first = false;
				limitTime = tNow;
			}

			if (realtime) {
				auto elapsedTick =
					std::chrono::duration_cast<std::chrono::milliseconds>(
						std::chrono::steady_clock::now() - tStartTick)
						.count();
				if (elapsedTick < 10) {
					std::this_thread::sleep_for(
						std::chrono::milliseconds(10 - elapsedTick));
				}
			}
		}

		std::cout << "\nDone. Ran " << done << " ticks." << std::endl;
	}
}

} // namespace GLStation::IO::Commands

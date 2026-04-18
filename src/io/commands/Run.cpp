#include "io/commands/Run.hpp"
#include "io/handlers/InputHandler.hpp"
#include "ui/Dashboard.hpp"
#include "ui/Terminal.hpp"
#include "util/CommandUtils.hpp"
#include <chrono>
#include <iostream>
#include <thread>

namespace GLStation::IO::Commands {

void Run::execute(Simulation::Engine &engine, const std::string &arg,
				  const std::string &extra) {
	bool realtime = false;
	if (!extra.empty()) {
		std::string extNorm = Util::InputHandler::normaliseForComparison(extra);
		if (extNorm == "realtime") {
			realtime = true;
		} else {
			std::cout << "Usage: run <time> [realtime] | run inf "
						 "[realtime]"
					  << std::endl;
			return;
		}
	}
	if (arg.empty()) {
		std::cout << "Usage: run <time> | run inf." << std::endl;
		return;
	}

	std::string argNorm = Util::InputHandler::normaliseForComparison(arg);
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
		if (!CommandUtils::parseRunDurationTicks(arg, durationTicks)) {
			std::cout << "Usage: run <time> [realtime] | run inf [realtime]"
					  << std::endl;
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

#include "grid/Breaker.hpp"
#include "grid/Generator.hpp"
#include "grid/Line.hpp"
#include "grid/Load.hpp"
#include "grid/Transformer.hpp"
#include "io/GridHandler.hpp"
#include "io/InputHandler.hpp"
#include "sim/Engine.hpp"
#include <cctype>
#include <chrono>
#include <cmath>
#include <deque>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {

static const char *ANSI_RESET = "\033[0m";
static const char *ANSI_GREEN = "\033[32m";
static const char *ANSI_YELLOW = "\033[33m";
static const char *ANSI_RED = "\033[31m";
static const char *ANSI_CYAN = "\033[36m";
static const int DASHBOARD_LINES = 10;
static const int DASHBOARD_INNER_WIDTH = 53;

/*
		Temporary TUI bandaid, theres a nicer way to do this while preventing dashboard flickering
*/
static int visibleLength(const std::string &s) {
	int n = 0;
	for (size_t i = 0; i < s.size(); ++i) {
		if (s[i] == '\033' && i + 1 < s.size() && s[i + 1] == '[') {
			size_t j = i + 2;
			while (j < s.size() && (s[j] < 0x40 || s[j] > 0x7E))
				++j;
			i = j;
		} else
			++n;
	}
	return n;
}

// we dont really need to bother with the colour stuff forever but still
static bool s_ansiEnabled = false;

static void enableAnsiIfPossible() {
	if (s_ansiEnabled)
		return;
#ifdef _WIN32
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut != INVALID_HANDLE_VALUE) {
		DWORD mode = 0;
		if (GetConsoleMode(hOut, &mode)) {
			mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
			if (SetConsoleMode(hOut, mode))
				s_ansiEnabled = true;
		}
	}
#else
	s_ansiEnabled = true;
#endif
}

static GLStation::Core::u64 parseTicksFromString(const std::string &arg) {
	if (arg.empty())
		return 1;
	try {
		if (arg.back() == 's' || arg.back() == 'S')
			return static_cast<GLStation::Core::u64>(
					   std::stoull(arg.substr(0, arg.size() - 1))) *
				   1000;
		if (arg.back() == 'm' || arg.back() == 'M')
			return static_cast<GLStation::Core::u64>(
					   std::stoull(arg.substr(0, arg.size() - 1))) *
				   60000;
		return static_cast<GLStation::Core::u64>(std::stoull(arg));
	} catch (...) {
	}
	return 1;
}

/*
	im not familiar with how CLI utilities usually do this, ill have to look at some OSS stuff
	this works for now but it is RANCID
*/
static bool shouldStopRun() {
#ifdef _WIN32
	HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
	if (hIn == INVALID_HANDLE_VALUE)
		return false;

	INPUT_RECORD rec;
	DWORD n = 0;
	if (PeekConsoleInput(hIn, &rec, 1, &n) && n > 0) {
		if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown) {
			ReadConsoleInput(hIn, &rec, 1, &n);
			char ch = rec.Event.KeyEvent.uChar.AsciiChar;
			return ch == 'x' || ch == 'X';
		}
		ReadConsoleInput(hIn, &rec, 1, &n);
	}
#endif
	return false;
}

/*
	this is intuitive  for me but theres other ways that might make more sense for some people
	please chime in if theres a smarter way to parse time
*/
static bool parseRunDurationTicks(const std::string &arg,
								  GLStation::Core::u64 &outTicks) {
	if (arg.empty())
		return false;
	std::string s = arg;
	for (char &c : s) {
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}

	long double factorMs = 1000.0L;
	size_t suffixLen = 0;
	if (s.size() >= 2 && s.substr(s.size() - 2) == "ms") {
		factorMs = 1.0L;
		suffixLen = 2;
	} else if (!s.empty() && s.back() == 's') {
		factorMs = 1000.0L;
		suffixLen = 1;
	} else if (!s.empty() && s.back() == 'm') {
		factorMs = 60000.0L;
		suffixLen = 1;
	} else if (!s.empty() && s.back() == 'h') {
		factorMs = 3600000.0L;
		suffixLen = 1;
	}

	std::string numPart =
		(suffixLen > 0) ? s.substr(0, s.size() - suffixLen) : s;
	if (numPart.empty())
		return false;

	try {
		long double value = std::stold(numPart);
		if (!(value >= 0))
			return false;
		long double ms = value * factorMs;
		if (ms < 1.0L)
			ms = 1.0L;

		outTicks = static_cast<GLStation::Core::u64>(
			std::llround(std::min<long double>(
				ms, static_cast<long double>(
						std::numeric_limits<GLStation::Core::u64>::max()))));
		return outTicks > 0;
	} catch (...) {
	}
	return false;
}

static const GLStation::Core::u64 RUN_DASH_INTERVAL_TICKS = 1000;

// Needs to be migrated to a module at some point
static void printLiveDashboard(const GLStation::Simulation::Engine &engine,
							   bool moveUp, bool showProgress,
							   GLStation::Core::u64 doneTicks,
							   GLStation::Core::u64 totalTicks) {
	static std::deque<double> s_freqHistory;
	const double freq = engine.getSystemFrequency();
	s_freqHistory.push_back(freq);
	while (s_freqHistory.size() > 28)
		s_freqHistory.pop_front();

	double vMinPu = 2.0, vMaxPu = 0.0, angMinDeg = 360.0, angMaxDeg = -360.0;
	std::string nameAtMinV;
	double worstLinePct = 0.0;
	std::string nameWorstLine;

	for (const auto &sub : engine.getSubstations()) {
		for (const auto &comp : sub->getComponents()) {
			if (auto *node =
					dynamic_cast<GLStation::Grid::Node *>(comp.get())) {
				std::complex<GLStation::Core::f64> v = node->getVoltage();
				double vPu = std::abs(v);
				double angDeg = std::arg(v) * 180.0 / GLStation::Core::PI;
				if (vPu < vMinPu) {
					vMinPu = vPu;
					nameAtMinV = node->getName();
				}
				if (vPu > vMaxPu)
					vMaxPu = vPu;
				if (angDeg < angMinDeg)
					angMinDeg = angDeg;
				if (angDeg > angMaxDeg)
					angMaxDeg = angDeg;
			}
			if (auto *line =
					dynamic_cast<GLStation::Grid::Line *>(comp.get())) {
				double limit = line->getCurrentLimit();
				if (limit > 1e-6) {
					double pct =
						100.0 * std::abs(line->getCurrentFlow()) / limit;
					if (pct > worstLinePct) {
						worstLinePct = pct;
						nameWorstLine = line->getName();
					}
				}
			}
			if (auto *trafo =
					dynamic_cast<GLStation::Grid::Transformer *>(comp.get())) {
				double limit = trafo->getCurrentLimit();
				if (limit > 1e-6) {
					double pct =
						100.0 * std::abs(trafo->getCurrentFlow()) / limit;
					if (pct > worstLinePct) {
						worstLinePct = pct;
						nameWorstLine = trafo->getName();
					}
				}
			}
		}
	}
	if (vMinPu > 1.5)
		vMinPu = 0.0;
	if (vMaxPu < 0.01)
		vMaxPu = 1.0;

	const char *freqColour = ANSI_RESET;
	if (freq >= 49.8 && freq <= 50.2)
		freqColour = ANSI_GREEN;
	else if (freq >= 49.5 && freq <= 50.5)
		freqColour = ANSI_YELLOW;
	else
		freqColour = ANSI_RED;

	const int barWidth = 22;
	const double fMin = 49.0, fMax = 51.0;
	int pos = static_cast<int>(
		std::round((freq - fMin) / (fMax - fMin) * (barWidth - 1)));
	if (pos < 0)
		pos = 0;
	if (pos >= barWidth)
		pos = barWidth - 1;

	std::string sparkline;
	if (s_freqHistory.size() >= 2) {
		double lo = 49.0, hi = 51.0;
		for (double f : s_freqHistory) {
			if (f < lo)
				lo = f;
			if (f > hi)
				hi = f;
		}
		if (hi - lo < 0.1)
			hi = lo + 0.1;
		const char asciiBar[] = " .:-=+*#%@";
		for (size_t i = 0; i < s_freqHistory.size(); ++i) {
			double v = (s_freqHistory[i] - lo) / (hi - lo);
			int idx = static_cast<int>(v * 8.99);
			if (idx < 0)
				idx = 0;
			if (idx > 8)
				idx = 8;
			sparkline += asciiBar[idx];
		}
	}

	auto fmtKw = [](double kw) -> std::string {
		std::ostringstream o;
		o << std::fixed << std::setprecision(1);
		if (kw >= 1e6)
			o << (kw / 1e6) << "M";
		else if (kw >= 1e3)
			o << (kw / 1e3) << "k";
		else
			o << kw;
		return o.str();
	};

	auto pad = [](const std::string &line) {
		int vis = visibleLength(line);
		if (vis < DASHBOARD_INNER_WIDTH)
			return line + std::string(DASHBOARD_INNER_WIDTH - vis, ' ');
		return line;
	};

	if (moveUp && s_ansiEnabled)
		std::cout << "\033[" << DASHBOARD_LINES << "A";

	const std::string borderTop =
		"+------------------ Live Simulation ------------------+";
	const std::string borderBottom =
		"+" + std::string(DASHBOARD_INNER_WIDTH, '-') + "+";

	std::ostringstream l1;
	l1 << " T " << engine.getTickCount() << " ms  "
	   << (s_ansiEnabled ? freqColour : "") << std::fixed
	   << std::setprecision(2) << freq << " Hz"
	   << (s_ansiEnabled ? ANSI_RESET : "");
	std::cout << (s_ansiEnabled ? ANSI_CYAN : "") << "  " << borderTop
			  << (s_ansiEnabled ? ANSI_RESET : "") << "\n";
	std::cout << "  |" << pad(l1.str()) << "|\n";

	std::ostringstream l2;
	l2 << " Gen " << fmtKw(engine.getTotalGeneration()) << "  Load "
	   << fmtKw(engine.getTotalLoad()) << "  Loss "
	   << fmtKw(engine.getTotalLosses()) << " kW";
	std::cout << "  |" << pad(l2.str()) << "|\n";

	std::ostringstream l3;
	l3 << " V " << std::fixed << std::setprecision(2) << vMinPu << "-" << vMaxPu
	   << " pu";
	if (!nameAtMinV.empty() && vMinPu < 0.98)
		l3 << " (low: " << nameAtMinV << ")";
	std::cout << "  |" << pad(l3.str()) << "|\n";

	std::ostringstream l4;
	l4 << " angle " << std::fixed << std::setprecision(1) << angMinDeg << ".."
	   << angMaxDeg << " deg";
	std::cout << "  |" << pad(l4.str()) << "|\n";

	std::ostringstream l5;
	l5 << " line " << std::fixed << std::setprecision(0) << worstLinePct << "%";
	if (!nameWorstLine.empty())
		l5 << " " << nameWorstLine;
	if (worstLinePct > 100.0)
		l5 << " (overload)";
	std::cout << "  |" << pad(l5.str()) << "|\n";

	std::ostringstream l6;
	for (int i = 0; i < barWidth; ++i)
		l6 << (i == pos ? (s_ansiEnabled ? "\033[1m*\033[0m" : "*") : "-");
	l6 << " " << fMin << "-" << fMax << " Hz";
	std::cout << "  |" << pad(l6.str()) << "|\n";

	std::cout << "  |"
			  << pad(" f " + sparkline + (sparkline.empty() ? "-" : ""))
			  << "|\n";

	if (showProgress && totalTicks > 0) {
		int pct = static_cast<int>((doneTicks * 100) / totalTicks);
		int filled = (pct * 18) / 100;
		std::ostringstream l8;
		l8 << " [";
		for (int i = 0; i < 18; ++i)
			l8 << (i < filled ? "#" : ".");
		l8 << "] " << std::setw(3) << pct << "%";
		std::cout << "  |" << pad(l8.str()) << "|\n";
	} else if (showProgress && totalTicks == 0) {
		std::cout << "  |" << pad(" [===  progress ===]") << "|\n";
	} else {
		std::cout << "  |" << std::string(DASHBOARD_INNER_WIDTH, ' ') << "|\n";
	}
	std::cout << "  " << borderBottom << "\n" << std::flush;
}

} // namespace

int main() {
	try {
		std::cout << "\n=============================================="
				  << std::endl;
		std::cout << "   GLSStation" << std::endl;
		std::cout << "==============================================\n"
				  << std::endl;

		GLStation::Simulation::Engine engine;
		engine.initialise();

		std::string line;
		while (true) {
			std::cout << "[" << engine.getTickCount() << " ms] > "
					  << std::flush;
			if (!std::getline(std::cin, line))
				break;
			line = GLStation::Util::InputHandler::trim(line);
			if (line.empty())
				continue;

			std::stringstream ss(line);
			std::string cmd;
			ss >> cmd;
			std::string cmdNorm =
				GLStation::Util::InputHandler::normaliseForComparison(cmd);

			if (cmdNorm == "exit")
				break;
			else if (cmdNorm == "help") {
				std::cout << "\nCommands\n"
						  << "==========\n\n"
						  << "exit\n"
						  << "tick [count]\n"
						  << "run <time>\n"
						  << "run inf\n"
						  << "status\n"
						  << "find <query>\n"
						  << "tree\n"
						  << "list <gen|load|line|trafo|breaker>\n"
						  << "inspect <id>\n"
						  << "set <id> <param> <value>\n"
						  << "open <id>\n"
						  << "close <id>\n"
						  << "export [filename]\n"
						  << "scenario list\n"
						  << "scenario add <open|close|set_load|set_gen> "
							 "<tick> <id> [kW]\n"
						  << "import <name>\n"
						  << std::endl;
			} else if (cmdNorm == "tick") {
				std::string arg;
				ss >> arg;
				GLStation::Core::u64 count = parseTicksFromString(arg);
				for (GLStation::Core::u64 i = 0; i < count; ++i)
					engine.tick();
				std::cout << "Advanced " << count << " ticks (" << count
						  << "ms sim time)." << std::endl;
			} else if (cmdNorm == "run") {
				std::string arg;
				std::string extra;
				ss >> arg;
				if (ss >> extra) {
					std::cout << "Usage: run <time> | run inf." << std::endl;
					continue;
				}
				if (arg.empty()) {
					std::cout << "Usage: run <time> | run inf." << std::endl;
					continue;
				}

				std::string argNorm =
					GLStation::Util::InputHandler::normaliseForComparison(arg);

				enableAnsiIfPossible();

				if (argNorm == "inf") {
					std::cout << "Run indefinitely. Press X to "
								 "stop.\n"
							  << std::endl;
					bool first = true;
					/*
					this is safe, i think
*/
					for (;;) {
						for (GLStation::Core::u64 i = 0;
							 i < RUN_DASH_INTERVAL_TICKS; ++i) {
							if (shouldStopRun())
								goto done_inf;
							engine.tick();
						}
						printLiveDashboard(engine, !first, true,
										   engine.getTickCount(), 0);
						first = false;
					}
				done_inf:
					printLiveDashboard(engine, false, false,
									   engine.getTickCount(), 0);
				} else {
					GLStation::Core::u64 durationTicks = 0;
					if (!parseRunDurationTicks(arg, durationTicks)) {
						std::cout << "Usage: run <time> | run inf."
								  << std::endl;
						continue;
					}
					std::cout << "Running for " << durationTicks
							  << " ticks. Press X to stop.\n"
							  << std::endl;

					bool first = true;
					GLStation::Core::u64 done = 0;
					for (; done < durationTicks;) {
						GLStation::Core::u64 chunk = std::min(
							RUN_DASH_INTERVAL_TICKS, durationTicks - done);
						for (GLStation::Core::u64 i = 0; i < chunk; ++i) {
							if (shouldStopRun()) {
								done = durationTicks;
								break;
							}
							engine.tick();
							++done;
						}
						printLiveDashboard(engine, (done > 0 && !first), true,
										   done, durationTicks);
						first = false;
					}

					std::cout << "\nDone. Ran " << done << " ticks."
							  << std::endl;
				}
			} else if (cmdNorm == "status") {
				std::cout << "\n--- SYSTEM OVERVIEW ---" << std::endl;
				std::cout << "Frequency:   " << std::setw(8) << std::fixed
						  << std::setprecision(3) << engine.getSystemFrequency()
						  << " Hz" << std::endl;
				std::cout << "Generation:  " << std::setw(8) << std::fixed
						  << std::setprecision(1) << engine.getTotalGeneration()
						  << " kW" << std::endl;
				std::cout << "Load:        " << std::setw(8)
						  << engine.getTotalLoad() << " kW" << std::endl;
				std::cout << "Losses:      " << std::setw(8)
						  << engine.getTotalLosses() << " kW ("
						  << (engine.getTotalGeneration() > 0
								  ? (engine.getTotalLosses() /
									 engine.getTotalGeneration() * 100)
								  : 0)
						  << "%)" << std::endl;
				std::cout << "Imbalance:   " << std::setw(8)
						  << (engine.getTotalGeneration() -
							  engine.getTotalLoad() - engine.getTotalLosses())
						  << " kW" << std::endl;
				std::cout << "-----------------------" << std::endl;
				for (const auto &sub : engine.getSubstations()) {
					std::cout << sub->toString() << std::endl;
				}
			} else if (cmdNorm == "find") {
				std::string query;
				ss >> query;
				query = GLStation::Util::InputHandler::trim(query);
				std::cout << "\nSearching for '" << query << "':" << std::endl;
				bool found = false;
				std::vector<std::pair<GLStation::Core::u64, std::string>>
					partial;
				std::string qNorm =
					GLStation::Util::InputHandler::normaliseForComparison(
						query);
				for (const auto &sub : engine.getSubstations()) {
					for (const auto &comp : sub->getComponents()) {
						std::string name = comp->getName();
						std::string nameNorm = GLStation::Util::InputHandler::
							normaliseForComparison(name);
						if (name.find(query) != std::string::npos ||
							(!query.empty() &&
							 nameNorm.find(qNorm) != std::string::npos)) {
							std::cout << "  - [ID: " << std::setw(2)
									  << comp->getId() << "] " << std::setw(15)
									  << name << " (" << typeid(*comp).name()
									  << ")" << std::endl;
							found = true;
						} else if (qNorm.size() >= 2 &&
								   nameNorm.find(qNorm) != std::string::npos)
							partial.push_back({comp->getId(), name});
					}
				}
				if (!found) {
					if (!partial.empty()) {
						std::cout << "  No exact match for \"" << query
								  << "\". Partial matches:" << std::endl;
						for (const auto &p : partial)
							std::cout << "  - [ID: " << std::setw(2) << p.first
									  << "] " << p.second << std::endl;
					} else {
						std::cout << "  No matches found." << std::endl;
					}
				}
			} else if (cmdNorm == "tree") {
				std::cout << "\n=== GRID TOPOLOGY ===" << std::endl;
				for (const auto &sub : engine.getSubstations()) {
					std::cout << "\n[Substation: " << sub->getName() << "]"
							  << std::endl;

					std::map<GLStation::Grid::Node *,
							 std::vector<GLStation::Grid::GridComponent *>>
						nodeMap;
					for (const auto &comp : sub->getComponents()) {
						if (auto node = dynamic_cast<GLStation::Grid::Node *>(
								comp.get())) {
							if (nodeMap.find(node) == nodeMap.end())
								nodeMap[node] = {};
						}
					}

					for (const auto &comp : sub->getComponents()) {
						if (auto gen =
								dynamic_cast<GLStation::Grid::Generator *>(
									comp.get()))
							nodeMap[gen->getConnectedNode()].push_back(
								comp.get());
						else if (auto load =
									 dynamic_cast<GLStation::Grid::Load *>(
										 comp.get()))
							nodeMap[load->getConnectedNode()].push_back(
								comp.get());
						else if (auto line =
									 dynamic_cast<GLStation::Grid::Line *>(
										 comp.get())) {
							nodeMap[line->getFromNode()].push_back(comp.get());
						} else if (auto trafo = dynamic_cast<
									   GLStation::Grid::Transformer *>(
									   comp.get())) {
							nodeMap[trafo->getPrimaryNode()].push_back(
								comp.get());
						} else if (auto breaker =
									   dynamic_cast<GLStation::Grid::Breaker *>(
										   comp.get())) {
							nodeMap[breaker->getFromNode()].push_back(
								comp.get());
						}
					}

					for (auto const &[node, components] : nodeMap) {
						std::cout << "  |-- " << node->getName() << " [Node "
								  << node->getId() << "]" << std::endl;
						for (auto *comp : components) {
							std::cout << "  |   +-- [ID: " << std::setw(2)
									  << comp->getId() << "] "
									  << comp->getName();
							if (auto b =
									dynamic_cast<GLStation::Grid::Breaker *>(
										comp))
								std::cout
									<< (b->isOpen() ? " (OPEN)" : " (CLOSED)");
							std::cout << std::endl;
						}
					}
				}
				std::cout << "\n==========================" << std::endl;
			} else if (cmdNorm == "list") {
				std::string type;
				ss >> type;
				type =
					GLStation::Util::InputHandler::normaliseForComparison(type);
				if (type.empty()) {
					std::cout << "Error: Missing type. Usage: list "
								 "<gen|load|line|trafo|breaker>"
							  << std::endl;
				} else {
					std::cout << "\nListing [" << type
							  << "] components:" << std::endl;
					bool validType =
						(type == "gen" || type == "load" || type == "line" ||
						 type == "trafo" || type == "breaker");
					if (!validType) {
						std::cout << "  Error: Unknown type \"" << type
								  << "\". Use: gen, load, line, trafo, breaker."
								  << std::endl;
					} else {
						int n = 0;
						for (const auto &sub : engine.getSubstations()) {
							for (const auto &comp : sub->getComponents()) {
								bool match = false;
								if (type == "gen" &&
									dynamic_cast<GLStation::Grid::Generator *>(
										comp.get()))
									match = true;
								if (type == "load" &&
									dynamic_cast<GLStation::Grid::Load *>(
										comp.get()))
									match = true;
								if (type == "line" &&
									dynamic_cast<GLStation::Grid::Line *>(
										comp.get()))
									match = true;
								if (type == "trafo" &&
									dynamic_cast<GLStation::Grid::Transformer
													 *>(comp.get()))
									match = true;
								if (type == "breaker" &&
									dynamic_cast<GLStation::Grid::Breaker *>(
										comp.get()))
									match = true;
								if (match) {
									std::cout << "  - [ID: " << comp->getId()
											  << "] " << comp->getName()
											  << std::endl;
									++n;
								}
							}
						}
						if (n == 0)
							std::cout << "  (none)" << std::endl;
					}
				}
			} else if (cmdNorm == "inspect") {
				GLStation::Core::u64 id;
				if (ss >> id) {
					bool found = false;
					for (const auto &sub : engine.getSubstations()) {
						for (const auto &comp : sub->getComponents()) {
							if (comp->getId() == id) {
								std::cout << "\n--- Component Inspect: "
										  << comp->getName() << " ---"
										  << std::endl;
								std::cout
									<< "Type:     " << typeid(*comp).name()
									<< std::endl;
								std::cout << "Summary:  " << comp->toString()
										  << std::endl;

								if (auto line =
										dynamic_cast<GLStation::Grid::Line *>(
											comp.get())) {
									std::cout << "Details:  Limit="
											  << line->getCurrentLimit() << "A"
											  << std::endl;
								} else if (auto gen = dynamic_cast<
											   GLStation::Grid::Generator *>(
											   comp.get())) {
									std::cout
										<< "Details:  P_target="
										<< gen->getTargetP()
										<< "kW, V_target=" << gen->getTargetV()
										<< "pu" << std::endl;
								} else if (auto load = dynamic_cast<
											   GLStation::Grid::Load *>(
											   comp.get())) {
									std::cout
										<< "Details:  P_max="
										<< load->getMaxPower()
										<< "kW, PF=" << load->getPowerFactor()
										<< std::endl;
								}
								found = true;
								break;
							}
						}
					}
					if (!found)
						std::cout << "No component with matching ID"
								  << std::endl;
				} else
					std::cout << "Usage: inspect <id>." << std::endl;
			} else if (cmdNorm == "set") {
				GLStation::Core::u64 id;
				std::string param;
				double val;
				if (ss >> id >> param >> val) {
					param =
						GLStation::Util::InputHandler::normaliseForComparison(
							param);
					bool ok = false;
					for (const auto &sub : engine.getSubstations()) {
						for (const auto &comp : sub->getComponents()) {
							if (comp->getId() == id) {
								if (auto gen = dynamic_cast<
										GLStation::Grid::Generator *>(
										comp.get())) {
									if (param == "p") {
										gen->setTargetP(val);
										ok = true;
									} else if (param == "v") {
										gen->setTargetV(val);
										ok = true;
									}
								} else if (auto load = dynamic_cast<
											   GLStation::Grid::Load *>(
											   comp.get())) {
									if (param == "pf") {
										load->setPowerFactor(val);
										ok = true;
									} else if (param == "p") {
										load->setMaxPower(val);
										ok = true;
									}
								} else if (auto line = dynamic_cast<
											   GLStation::Grid::Line *>(
											   comp.get())) {
									if (param == "limit") {
										line->setCurrentLimit(val);
										ok = true;
									}
								} else if (auto trafo = dynamic_cast<
											   GLStation::Grid::Transformer *>(
											   comp.get())) {
									if (param == "limit") {
										trafo->setCurrentLimit(val);
										ok = true;
									} else if (param == "tap") {
										trafo->setTap(val);
										ok = true;
									}
								}
								if (ok)
									std::cout << "Param" << comp->getName()
											  << " " << param << " set to "
											  << val << std::endl;
								break;
							}
						}
					}
					if (!ok) {
						bool hasId = false;
						for (const auto &sub : engine.getSubstations())
							for (const auto &comp : sub->getComponents())
								if (comp->getId() == id) {
									hasId = true;
									break;
								}
						if (!hasId)
							std::cout << "Error: ID " << id << std::endl;
						else
							std::cout << "Parameter '" << param
									  << "' not allowed for this component."
										 "(gen: p,v; load: p,pf; line/trafo: "
										 "limit; trafo: tap)"
									  << std::endl;
					}
				} else
					std::cout << "Usage: set <id> <param> <value>."
							  << std::endl;
			} else if (cmdNorm == "open" || cmdNorm == "close") {
				GLStation::Core::u64 id;
				if (ss >> id) {
					bool found = false;
					for (const auto &sub : engine.getSubstations()) {
						for (const auto &comp : sub->getComponents()) {
							if (comp->getId() == id) {
								if (auto breaker = dynamic_cast<
										GLStation::Grid::Breaker *>(
										comp.get())) {
									breaker->setOpen(cmdNorm == "open");
									std::cout << "Breaker #" << id << " ("
											  << comp->getName() << ") is now "
											  << (breaker->isOpen() ? "OPEN"
																	: "CLOSED")
											  << std::endl;
								} else
									std::cout << "Error: ID " << id
											  << " is not a breaker."
											  << std::endl;
								found = true;
								break;
							}
						}
					}
					if (!found)
						std::cout << "No component with ID " << id << std::endl;
				} else
					std::cout << "Usage: open <id>  or  close <id>"
							  << std::endl;
			} else if (cmdNorm == "export") {
				std::string filename = "results.csv";
				ss >> filename;
				engine.exportVoltagesToCSV(filename);
				std::cout << "Exported to " << filename << std::endl;
			} else if (cmdNorm == "scenario") {
				std::string sub;
				ss >> sub;
				sub =
					GLStation::Util::InputHandler::normaliseForComparison(sub);
				if (sub == "list") {
					auto ticks =
						engine.getScenarioManager().getScheduledTicks();
					if (ticks.empty())
						std::cout << "No scheduled scenarios." << std::endl;
					else {
						std::cout << "Scheduled events:" << std::endl;
						for (GLStation::Core::u64 t : ticks) {
							std::cout << "  T+" << t << " (" << t
									  << " ms sim time)" << std::endl;
						}
					}
				} else if (sub == "add") {
					std::string type;
					GLStation::Core::u64 tick, id;
					ss >> type >> tick >> id;
					type =
						GLStation::Util::InputHandler::normaliseForComparison(
							type);
					if (type == "open") {
						engine.getScenarioManager().addEvent(tick, [&engine,
																	id]() {
							if (engine.openBreakerById(id))
								std::cout << "\n[SCENARIO] Breaker " << id
										  << " opened (e.g. line fault) at T="
										  << engine.getTickCount() << " ms.\n"
										  << std::flush;
							else
								std::cout << "\n[SCENARIO] Breaker ID " << id
										  << " not found.\n"
										  << std::flush;
						});
						std::cout << "At T+" << tick << " ms will open breaker "
								  << id << "." << std::endl;
					} else if (type == "close") {
						engine.getScenarioManager().addEvent(tick, [&engine,
																	id]() {
							if (engine.closeBreakerById(id))
								std::cout
									<< "\n[SCENARIO] Breaker " << id
									<< " closed at T=" << engine.getTickCount()
									<< " ms.\n"
									<< std::flush;
							else
								std::cout << "\n[SCENARIO] Breaker ID " << id
										  << " not found.\n"
										  << std::flush;
						});
						std::cout << "At T+" << tick << "ms will close breaker "
								  << id << "." << std::endl;
					} else if (type == "set_load") {
						double kw;
						if (ss >> kw) {
							engine.getScenarioManager().addEvent(
								tick, [&engine, id, kw]() {
									if (engine.setLoadPowerById(id, kw))
										std::cout
											<< "\n[SCENARIO] Load " << id
											<< " set to " << kw << " kW at T="
											<< engine.getTickCount() << " ms.\n"
											<< std::flush;
									else
										std::cout << "\n[SCENARIO] Load ID "
												  << id << " not found.\n"
												  << std::flush;
								});
							std::cout << "At T+" << tick << " ms will set load "
									  << id << " to " << kw << " kW."
									  << std::endl;
						} else
							std::cout << "Error: scenario add set_load <tick> "
										 "<load_id> <kW>"
									  << std::endl;
					} else if (type == "set_gen") {
						double kw;
						if (ss >> kw) {
							engine.getScenarioManager().addEvent(
								tick, [&engine, id, kw]() {
									if (engine.setGenTargetPById(id, kw))
										std::cout
											<< "\n[SCENARIO] Generator " << id
											<< " setpoint " << kw << " kW at T="
											<< engine.getTickCount() << " ms.\n"
											<< std::flush;
									else
										std::cout
											<< "\n[SCENARIO] Generator ID "
											<< id << " not found.\n"
											<< std::flush;
								});
							std::cout << "At T+" << tick
									  << " ms will set generator " << id
									  << " to " << kw << " kW." << std::endl;
						} else
							std::cout
								<< "scenario add set_gen <tick> <gen_id> <kW>"
								<< std::endl;
					} else
						std::cout
							<< "scenario add <open|close|set_load|set_gen> "
							   "<tick> <id> [kW]."
							<< std::endl;
				} else if (!sub.empty())
					std::cout << "Use scenario add ... or scenario list."
							  << std::endl;
				else
					std::cout << "Usage: scenario list | scenario add <type> "
								 "<tick> <id> [kW]"
							  << std::endl;
			} else if (cmdNorm == "import") {
				std::string cityName;
				std::getline(ss, cityName);
				cityName = GLStation::Util::InputHandler::trim(cityName);
				if (!cityName.empty()) {
					if (GLStation::Simulation::GridHandler::importCity(
							cityName)) {
						engine.initialise();
						std::cout << "Grid loaded for: " << cityName
								  << std::endl;
					} else {
						auto suggestions =
							GLStation::Simulation::GridHandler::getSuggestions(
								cityName);
						if (!suggestions.empty()) {
							std::cout << "\nNo exact match for \"" << cityName
									  << "\". Closest matches:" << std::endl;
							for (size_t i = 0; i < suggestions.size(); ++i)
								std::cout << "  " << (i + 1) << ". "
										  << suggestions[i] << std::endl;
							std::cout << "Pick (1-" << suggestions.size()
									  << ") or cancel: " << std::flush;
							std::string choice;
							if (std::getline(std::cin, choice)) {
								choice =
									GLStation::Util::InputHandler::trim(choice);
								size_t idx = 0;
								try {
									idx =
										static_cast<size_t>(std::stoul(choice));
								} catch (...) {
								}
								if (idx >= 1 && idx <= suggestions.size()) {
									if (GLStation::Simulation::GridHandler::
											importCity(suggestions[idx - 1])) {
										engine.initialise();
										std::cout << "Grid loaded for: "
												  << suggestions[idx - 1]
												  << std::endl;
									}
								}
							}
						} else {
							std::cout << "No matching area in OpenStreetMap or "
										 "internet "
										 "error idk didnt account for that."
									  << std::endl;
							std::cout
								<< "Use 'import <name>' with exact spelling."
								<< std::endl;
						}
					}
				} else {
					std::cout << "Usage: import [Name]" << std::endl;
				}
			} else {
				std::cout << "Error: Unknown Command '" << cmd << "'."
						  << std::endl;
			}
		}

	} catch (const std::exception &e) {
		std::cerr << "FUUUCK: " << e.what() << std::endl;
	}
	/*
	^thought about changing this but never had a runtime exception so lets leave it as an easter egg
*/
	std::cout << "Exiting..." << std::endl;
	return 0;
}

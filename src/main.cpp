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
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace {

static const char *ANSI_RESET = "\033[0m";
static const char *ANSI_GREEN = "\033[32m";
static const char *ANSI_YELLOW = "\033[33m";
static const char *ANSI_RED = "\033[31m";
static const char *ANSI_CYAN = "\033[36m";
static const int DASHBOARD_LINES = 12;
static const int DASHBOARD_INNER_WIDTH = 75;

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

struct ScopedRawStdin {
#ifndef _WIN32
	bool active = false;
	bool isTty = false;
	int oldFlags = -1;
	termios oldTio{};

	ScopedRawStdin() {
		isTty = ::isatty(STDIN_FILENO) == 1;
		if (!isTty)
			return;

		if (::tcgetattr(STDIN_FILENO, &oldTio) != 0)
			return;

		termios rawTio = oldTio;
		rawTio.c_lflag &= ~static_cast<tcflag_t>(ICANON | ECHO);
		rawTio.c_cc[VMIN] = 0;
		rawTio.c_cc[VTIME] = 0;

		if (::tcsetattr(STDIN_FILENO, TCSANOW, &rawTio) != 0)
			return;

		oldFlags = ::fcntl(STDIN_FILENO, F_GETFL, 0);
		if (oldFlags != -1)
			(void)::fcntl(STDIN_FILENO, F_SETFL, oldFlags | O_NONBLOCK);

		active = true;
	}

	~ScopedRawStdin() {
		if (!active)
			return;
		(void)::tcsetattr(STDIN_FILENO, TCSANOW, &oldTio);
		if (oldFlags != -1)
			(void)::fcntl(STDIN_FILENO, F_SETFL, oldFlags);
	}
#endif
};

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
#else
	if (::isatty(STDIN_FILENO) != 1)
		return false;

	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(STDIN_FILENO, &rfds);
	timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	int ready = ::select(STDIN_FILENO + 1, &rfds, nullptr, nullptr, &tv);
	if (ready <= 0)
		return false;

	unsigned char ch = 0;
	ssize_t n = ::read(STDIN_FILENO, &ch, 1);
	if (n == 1)
		return ch == 'x' || ch == 'X';
	if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
		return false;
	return false;
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
	(void)showProgress;
	(void)doneTicks;
	(void)totalTicks;
	static std::deque<double> s_freqHistory;
	const double freq = engine.getSystemFrequency();
	const double nominalHz = engine.getNominalHz();
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
	if (freq >= nominalHz - 0.2 && freq <= nominalHz + 0.2)
		freqColour = ANSI_GREEN;
	else if (freq >= nominalHz - 0.5 && freq <= nominalHz + 0.5)
		freqColour = ANSI_YELLOW;
	else
		freqColour = ANSI_RED;

	const int barWidth = 22;
	const double fMin = nominalHz - 1.0, fMax = nominalHz + 1.0;
	int pos = static_cast<int>(
		std::round((freq - fMin) / (fMax - fMin) * (barWidth - 1)));
	if (pos < 0)
		pos = 0;
	if (pos >= barWidth)
		pos = barWidth - 1;

	std::string sparkline;
	if (s_freqHistory.size() >= 2) {
		double lo = nominalHz - 1.0, hi = nominalHz + 1.0;
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

	if (moveUp && s_ansiEnabled)
		std::cout << "\033[" << DASHBOARD_LINES << "A";

	auto fixLen = [](std::string s, int len) {
		int vis = visibleLength(s);
		if (vis < len)
			return s + std::string(len - vis, ' ');
		if (vis > len) {
			return s.substr(0, len);
		}
		return s;
	};

	auto drawBar = [](double val, double maxVal) {
		if (maxVal < 1e-6)
			maxVal = 1.0;
		double ratio = val / maxVal;
		if (ratio > 1.0)
			ratio = 1.0;
		if (ratio < 0.0)
			ratio = 0.0;
		int width = 11;
		int filled = static_cast<int>(std::round(ratio * width));
		std::string bg = s_ansiEnabled ? "\033[90m-\033[0m" : "-";
		std::string fg = s_ansiEnabled ? "\033[36m#\033[0m" : "#";
		std::string res = "[";
		for (int i = 0; i < width; ++i)
			res += (i < filled) ? fg : bg;
		res += "]";
		return res;
	};

	double maxW = engine.getTotalGeneration() > engine.getTotalLoad()
					  ? engine.getTotalGeneration()
					  : engine.getTotalLoad();
	if (maxW < 1.0)
		maxW = 1.0;

	std::string topBorder = "+" + std::string(DASHBOARD_INNER_WIDTH, '-') + "+";
	std::string midBorder = "+---------------+---------------------------------"
							"-+------------------------+";

	std::string cCol = s_ansiEnabled ? ANSI_CYAN : "";
	std::string cRes = s_ansiEnabled ? ANSI_RESET : "";
	std::string bL = "  " + cCol + "|" + cRes;
	std::string bM = cCol + "|" + cRes;
	std::string bR = cCol + "|\n" + cRes;

	std::cout << cCol << "  " << topBorder << cRes << "\n";

	GLStation::Core::u64 t = engine.getTickCount();
	GLStation::Core::u64 hrs = (t / 1000) / 3600;
	GLStation::Core::u64 mins = ((t / 1000) % 3600) / 60;
	GLStation::Core::u64 secs = (t / 1000) % 60;
	std::ostringstream timeStr;
	timeStr << std::setfill('0') << std::setw(2) << hrs << "h "
			<< std::setfill('0') << std::setw(2) << mins << "m "
			<< std::setfill('0') << std::setw(2) << secs << "s";

	std::ostringstream leftHdr;
	leftHdr << (s_ansiEnabled ? ANSI_CYAN : "") << " Live Grid Simulation"
			<< (s_ansiEnabled ? ANSI_RESET : "");
	std::ostringstream rightHdr;
	rightHdr << timeStr.str() << " | T:" << engine.getTickCount() << " ";
	int spaces = DASHBOARD_INNER_WIDTH - visibleLength(leftHdr.str()) -
				 visibleLength(rightHdr.str());
	if (spaces < 0)
		spaces = 0;
	std::cout << bL << leftHdr.str() << std::string(spaces, ' ')
			  << rightHdr.str() << bR;
	std::cout << cCol << "  " << midBorder << "\n" << cRes;

	const char *vColor = ANSI_GREEN;
	if (vMinPu < 0.90 || vMaxPu > 1.10)
		vColor = ANSI_RED;
	else if (vMinPu < 0.95 || vMaxPu > 1.05)
		vColor = ANSI_YELLOW;

	const char *lColor = ANSI_GREEN;
	if (worstLinePct >= 100.0)
		lColor = ANSI_RED;
	else if (worstLinePct >= 80.0)
		lColor = ANSI_YELLOW;

	std::ostringstream f1, p1, h1;
	f1 << "  " << (s_ansiEnabled ? freqColour : "") << std::fixed
	   << std::setprecision(2) << freq << " Hz"
	   << (s_ansiEnabled ? ANSI_RESET : "");
	p1 << "  " << (s_ansiEnabled ? ANSI_GREEN : "")
	   << "[+] Gen:  " << std::setw(7) << fmtKw(engine.getTotalGeneration())
	   << (s_ansiEnabled ? ANSI_RESET : "") << " "
	   << drawBar(engine.getTotalGeneration(), maxW);
	h1 << "  " << (s_ansiEnabled ? vColor : "") << "V: " << std::fixed
	   << std::setprecision(2) << vMinPu << "-" << vMaxPu << " pu"
	   << (s_ansiEnabled ? ANSI_RESET : "");
	std::cout << bL << fixLen(f1.str(), 15) << bM << fixLen(p1.str(), 34) << bM
			  << fixLen(h1.str(), 24) << bR;

	std::ostringstream f2, p2, h2;
	f2 << "  Nadir: " << std::fixed << std::setprecision(2)
	   << engine.getFrequencyNadirLifetime();
	p2 << "  " << (s_ansiEnabled ? ANSI_YELLOW : "")
	   << "[-] Load: " << std::setw(7) << fmtKw(engine.getTotalLoad())
	   << (s_ansiEnabled ? ANSI_RESET : "") << " "
	   << drawBar(engine.getTotalLoad(), maxW);
	h2 << "  ang: " << std::fixed << std::setprecision(1) << angMinDeg << ".."
	   << angMaxDeg << " deg";
	std::cout << bL << fixLen(f2.str(), 15) << bM << fixLen(p2.str(), 34) << bM
			  << fixLen(h2.str(), 24) << bR;

	std::ostringstream f3, p3, h3;
	f3 << "  L " << std::fixed << std::setprecision(1) << fMin << " H " << fMax;
	p3 << "  " << (s_ansiEnabled ? ANSI_RED : "")
	   << "[-] Loss: " << std::setw(7) << fmtKw(engine.getTotalLosses())
	   << (s_ansiEnabled ? ANSI_RESET : "") << " "
	   << drawBar(engine.getTotalLosses(), maxW);

	double absMin = std::abs(angMinDeg);
	double absMax = std::abs(angMaxDeg);
	double maxAbsAng = (absMin > absMax) ? absMin : absMax;
	int angPos = static_cast<int>(maxAbsAng * 8.0 / 2.0);
	if (angPos < 0)
		angPos = 0;
	if (angPos > 7)
		angPos = 7;
	std::string angSpark = "[-";
	for (int i = 0; i < 8; ++i)
		angSpark +=
			(i == angPos ? (s_ansiEnabled ? "\033[1m*\033[0m" : "*") : "-");
	angSpark += "]";
	h3 << "  " << angSpark;

	std::cout << bL << fixLen(f3.str(), 15) << bM << fixLen(p3.str(), 34) << bM
			  << fixLen(h3.str(), 24) << bR;

	std::ostringstream f4, p4, h4;
	std::string fSpark = "[-";
	int mappedPos = static_cast<int>(pos * 8.0 / barWidth);
	for (int i = 0; i < 8; ++i)
		fSpark +=
			(i == mappedPos ? (s_ansiEnabled ? "\033[1m*\033[0m" : "*") : "-");
	fSpark += "]";
	f4 << "  " << fSpark;
	double imb = engine.getTotalGeneration() - engine.getTotalLoad() -
				 engine.getTotalLosses();
	const char *imbColor = (std::abs(imb) > 5.0) ? ANSI_YELLOW : ANSI_RESET;
	p4 << "  " << (s_ansiEnabled ? imbColor : "")
	   << "[=] Imb:  " << std::setw(7) << fmtKw(imb)
	   << (s_ansiEnabled ? ANSI_RESET : "");
	std::string lineInfo = nameWorstLine.empty() ? "None" : nameWorstLine;
	if (lineInfo.size() > 10)
		lineInfo = lineInfo.substr(0, 10);
	h4 << "  Line: " << lineInfo;
	std::cout << bL << fixLen(f4.str(), 15) << bM << fixLen(p4.str(), 34) << bM
			  << fixLen(h4.str(), 24) << bR;

	std::ostringstream f5, p5, h5;
	f5 << " ";
	p5 << " ";
	h5 << "  " << (s_ansiEnabled ? lColor : "") << std::fixed
	   << std::setprecision(1) << worstLinePct << "% "
	   << (s_ansiEnabled ? ANSI_RESET : "") << drawBar(worstLinePct, 100.0)
	   << (worstLinePct > 100.0 ? "!" : "");
	std::cout << bL << fixLen(f5.str(), 15) << bM << fixLen(p5.str(), 34) << bM
			  << fixLen(h5.str(), 24) << bR;

	std::cout << cCol << "  " << midBorder << "\n" << cRes;

	std::string lastEvent = engine.getLastEvent();
	if (lastEvent.empty())
		lastEvent = "Simulation nominal.";
	if (static_cast<int>(lastEvent.size()) > DASHBOARD_INNER_WIDTH - 6)
		lastEvent = lastEvent.substr(0, DASHBOARD_INNER_WIDTH - 9) + "...";
	std::cout << bL << fixLen("  Log:", DASHBOARD_INNER_WIDTH) << bR;
	std::cout << bL << fixLen("  " + lastEvent, DASHBOARD_INNER_WIDTH) << bR;

	std::cout << cCol << "  " << topBorder << "\n" << cRes << std::flush;
}

} // namespace

int main() {
	enableAnsiIfPossible();
	try {
		std::string cyn = s_ansiEnabled ? ANSI_CYAN : "";
		std::string res = s_ansiEnabled ? ANSI_RESET : "";

		std::cout << "\n"
				  << cyn << "   GLStation\n"
				  << res << "   Type 'help' for commands.\n\n";

		GLStation::Simulation::Engine engine;
		engine.initialise();

		std::string line;
		while (true) {
			std::cout << cyn << "glstation" << res << "@"
					  << engine.getTickCount() << "ms > " << std::flush;
			line = GLStation::Util::InputHandler::readLineWithHistory();
			if (line == "exit" && !std::cin.good())
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
				std::string cyn = s_ansiEnabled ? ANSI_CYAN : "";
				std::string res = s_ansiEnabled ? ANSI_RESET : "";
				std::string yel = s_ansiEnabled ? ANSI_YELLOW : "";
				std::string grn = s_ansiEnabled ? ANSI_GREEN : "";
				std::cout << "\n"
						  << cyn << "--- Commands ---" << res << "\n\n"
						  << grn << "  exit" << res << "\n"
						  << grn << "  tick " << yel << "[count]" << res << "\n"
						  << grn << "  run " << yel << "<time> [realtime]"
						  << res << "\n"
						  << grn << "  run inf " << yel << "[realtime]" << res
						  << "\n"
						  << grn << "  status" << res << "\n"
						  << grn << "  find " << yel << "<query>" << res << "\n"
						  << grn << "  tree" << res << "\n"
						  << grn << "  list " << yel
						  << "<gen|load|line|trafo|breaker>" << res << "\n"
						  << grn << "  inspect " << yel << "<id>" << res << "\n"
						  << grn << "  set " << yel << "<id> <param> <value>"
						  << res << "\n"
						  << grn << "  open " << yel << "<id>" << res << "\n"
						  << grn << "  close " << yel << "<id>" << res << "\n"
						  << grn << "  export " << yel << "[filename]" << res
						  << "\n"
						  << grn << "  import " << yel << "<name>" << res
						  << "\n"
						  << grn << "  import demo" << res << "\n"
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
				bool realtime = false;
				if (ss >> extra) {
					std::string extNorm =
						GLStation::Util::InputHandler::normaliseForComparison(
							extra);
					if (extNorm == "realtime") {
						realtime = true;
					} else {
						std::cout << "Usage: run <time> [realtime] | run inf "
									 "[realtime]"
								  << std::endl;
						continue;
					}
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
					ScopedRawStdin rawInput;
					(void)rawInput;
					bool first = true;
					/*
					this is safe, i think
*/
					for (;;) {
						auto tStart = std::chrono::steady_clock::now();
						for (GLStation::Core::u64 i = 0;
							 i < RUN_DASH_INTERVAL_TICKS; ++i) {
							if (shouldStopRun())
								goto done_inf;
							engine.tick();
						}
						printLiveDashboard(engine, !first, true,
										   engine.getTickCount(), 0);
						first = false;
						if (realtime) {
							auto tEnd = std::chrono::steady_clock::now();
							auto elapsed =
								std::chrono::duration_cast<
									std::chrono::milliseconds>(tEnd - tStart)
									.count();
							if (elapsed < static_cast<long long>(
											  RUN_DASH_INTERVAL_TICKS)) {
								std::this_thread::sleep_for(
									std::chrono::milliseconds(
										RUN_DASH_INTERVAL_TICKS - elapsed));
							}
						}
					}
				done_inf:
					printLiveDashboard(engine, !first, false,
									   engine.getTickCount(), 0);
				} else {
					GLStation::Core::u64 durationTicks = 0;
					if (!parseRunDurationTicks(arg, durationTicks)) {
						std::cout << "Usage: run <time> [realtime] | run inf "
									 "[realtime]"
								  << std::endl;
						continue;
					}
					std::cout << "Running for " << durationTicks
							  << " ticks. Press X to stop.\n"
							  << std::endl;

					ScopedRawStdin rawInput;
					(void)rawInput;
					bool first = true;
					GLStation::Core::u64 done = 0;
					for (; done < durationTicks;) {
						auto tStart = std::chrono::steady_clock::now();
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
						if (realtime && chunk > 0) {
							auto tEnd = std::chrono::steady_clock::now();
							auto elapsed =
								std::chrono::duration_cast<
									std::chrono::milliseconds>(tEnd - tStart)
									.count();
							if (elapsed < static_cast<long long>(chunk)) {
								std::this_thread::sleep_for(
									std::chrono::milliseconds(chunk - elapsed));
							}
						}
					}

					std::cout << "\nDone. Ran " << done << " ticks."
							  << std::endl;
				}
			} else if (cmdNorm == "status") {
				std::string cyn = s_ansiEnabled ? ANSI_CYAN : "";
				std::string res = s_ansiEnabled ? ANSI_RESET : "";
				std::cout << "\n"
						  << cyn << "--- System Overview ---" << res
						  << std::endl;
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
				std::cout << "Freq Nadir:  " << std::setw(8)
						  << engine.getFrequencyNadirLifetime() << " Hz"
						  << std::endl;
				std::cout << "Max line:    " << std::setw(8)
						  << engine.getMaxLineLoadingLifetime() << " %"
						  << std::endl;
				std::cout << "Shed load:   " << std::setw(8)
						  << engine.getActiveShedLoadKw() << " kW" << std::endl;
				std::cout << "Reserve:     " << std::setw(8)
						  << engine.getReserveMarginKw() << " kW" << std::endl;
				std::cout << cyn << "-----------------------" << res
						  << std::endl;
				for (const auto &sub : engine.getSubstations()) {
					std::cout << sub->toString() << std::endl;
				}
			} else if (cmdNorm == "find") {
				std::string query;
				ss >> query;
				query = GLStation::Util::InputHandler::trim(query);
				std::string cyn = s_ansiEnabled ? ANSI_CYAN : "";
				std::string res = s_ansiEnabled ? ANSI_RESET : "";
				std::cout << "\n"
						  << cyn << "--- Search: '" << query << "' ---" << res
						  << std::endl;
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
				std::string cyn = s_ansiEnabled ? ANSI_CYAN : "";
				std::string res = s_ansiEnabled ? ANSI_RESET : "";
				std::string yel = s_ansiEnabled ? ANSI_YELLOW : "";
				std::cout << "\n"
						  << cyn << "--- Grid Topology ---" << res << std::endl;
				for (const auto &sub : engine.getSubstations()) {
					std::cout << "\n"
							  << yel << "[Substation:" << res << " "
							  << sub->getName() << yel << "]" << res
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
						std::cout << cyn << "  |-- " << res << node->getName()
								  << " [Node " << node->getId() << "]"
								  << std::endl;
						for (auto *comp : components) {
							std::cout << cyn << "  |   +-- " << res
									  << "[ID: " << std::setw(2)
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
				std::cout << std::endl;
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
					std::string cyn = s_ansiEnabled ? ANSI_CYAN : "";
					std::string res = s_ansiEnabled ? ANSI_RESET : "";
					std::cout << "\n"
							  << cyn << "--- List [" << type << "] ---" << res
							  << std::endl;
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
								std::string cyn =
									s_ansiEnabled ? ANSI_CYAN : "";
								std::string res =
									s_ansiEnabled ? ANSI_RESET : "";
								std::cout << "\n"
										  << cyn << "--- Component Inspect: "
										  << comp->getName() << " ---" << res
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
			} else if (cmdNorm == "import") {
				std::string cityName;
				std::getline(ss, cityName);
				cityName = GLStation::Util::InputHandler::trim(cityName);
				if (!cityName.empty()) {
					std::string cityNorm =
						GLStation::Util::InputHandler::normaliseForComparison(
							cityName);
					if (cityNorm == "demo") {
						std::error_code ec;
						(void)std::filesystem::remove("grid.csv", ec);
						engine.initialise();
						std::cout << "Demo grid loaded." << std::endl;
						continue;
					}

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
					std::cout << "Usage: import <name> | import demo"
							  << std::endl;
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

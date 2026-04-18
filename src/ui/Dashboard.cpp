#include "grid/Line.hpp"
#include "grid/Node.hpp"
#include "grid/Transformer.hpp"
#include "sim/Engine.hpp"
#include "ui/Dashboard.hpp"
#include "ui/Terminal.hpp"
#include <cmath>
#include <complex>
#include <deque>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace GLStation::UI {

static const int DASHBOARD_LINES = 12;
static const int DASHBOARD_INNER_WIDTH = 75;

// Needs to be migrated to a module at some point
void printLiveDashboard(const Simulation::Engine &engine, bool moveUp,
						bool showProgress, Core::u64 doneTicks,
						Core::u64 totalTicks) {
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
			if (auto *node = dynamic_cast<Grid::Node *>(comp.get())) {
				std::complex<Core::f64> v = node->getVoltage();
				double vPu = std::abs(v);
				double angDeg = std::arg(v) * 180.0 / Core::PI;
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
			if (auto *line = dynamic_cast<Grid::Line *>(comp.get())) {
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
			if (auto *trafo = dynamic_cast<Grid::Transformer *>(comp.get())) {
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

	std::ostringstream out;

	if (moveUp && isAnsiEnabled()) {
		out << "\033[?25l"; // hide cursor
		out << "\033[" << DASHBOARD_LINES << "A";
	}

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
		std::string bg = isAnsiEnabled() ? "\033[90m-\033[0m" : "-";
		std::string fg = isAnsiEnabled() ? "\033[36m#\033[0m" : "#";
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

	std::string cCol = isAnsiEnabled() ? ANSI_CYAN : "";
	std::string cRes = isAnsiEnabled() ? ANSI_RESET : "";
	std::string bL = "  " + cCol + "|" + cRes;
	std::string bM = cCol + "|" + cRes;
	std::string bR = cCol + "|\n" + cRes;

	out << cCol << "  " << topBorder << cRes << "\n";

	Core::u64 t = engine.getTickCount();
	Core::u64 hrs = (t / 1000) / 3600;
	Core::u64 mins = ((t / 1000) % 3600) / 60;
	Core::u64 secs = (t / 1000) % 60;
	std::ostringstream timeStr;
	timeStr << std::setfill('0') << std::setw(2) << hrs << "h "
			<< std::setfill('0') << std::setw(2) << mins << "m "
			<< std::setfill('0') << std::setw(2) << secs << "s";

	std::ostringstream leftHdr;
	leftHdr << (isAnsiEnabled() ? ANSI_CYAN : "") << " Live Grid Simulation"
			<< (isAnsiEnabled() ? ANSI_RESET : "");
	std::ostringstream rightHdr;
	rightHdr << timeStr.str() << " | T:" << engine.getTickCount() << " ";
	int spaces = DASHBOARD_INNER_WIDTH - visibleLength(leftHdr.str()) -
				 visibleLength(rightHdr.str());
	if (spaces < 0)
		spaces = 0;
	out << bL << leftHdr.str() << std::string(spaces, ' ') << rightHdr.str()
		<< bR;
	out << cCol << "  " << midBorder << "\n" << cRes;

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
	f1 << "  " << (isAnsiEnabled() ? freqColour : "") << std::fixed
	   << std::setprecision(2) << freq << " Hz"
	   << (isAnsiEnabled() ? ANSI_RESET : "");
	p1 << "  " << (isAnsiEnabled() ? ANSI_GREEN : "")
	   << "[+] Gen:  " << std::setw(7) << fmtKw(engine.getTotalGeneration())
	   << (isAnsiEnabled() ? ANSI_RESET : "") << " "
	   << drawBar(engine.getTotalGeneration(), maxW);
	h1 << "  " << (isAnsiEnabled() ? vColor : "") << "V: " << std::fixed
	   << std::setprecision(2) << vMinPu << "-" << vMaxPu << " pu"
	   << (isAnsiEnabled() ? ANSI_RESET : "");
	out << bL << fixLen(f1.str(), 15) << bM << fixLen(p1.str(), 34) << bM
		<< fixLen(h1.str(), 24) << bR;

	std::ostringstream f2, p2, h2;
	f2 << "  Nadir: " << std::fixed << std::setprecision(2)
	   << engine.getFrequencyNadirLifetime();
	p2 << "  " << (isAnsiEnabled() ? ANSI_YELLOW : "")
	   << "[-] Load: " << std::setw(7) << fmtKw(engine.getTotalLoad())
	   << (isAnsiEnabled() ? ANSI_RESET : "") << " "
	   << drawBar(engine.getTotalLoad(), maxW);
	h2 << "  ang: " << std::fixed << std::setprecision(1) << angMinDeg << ".."
	   << angMaxDeg << " deg";
	out << bL << fixLen(f2.str(), 15) << bM << fixLen(p2.str(), 34) << bM
		<< fixLen(h2.str(), 24) << bR;

	std::ostringstream f3, p3, h3;
	f3 << "  L " << std::fixed << std::setprecision(1) << fMin << " H " << fMax;
	p3 << "  " << (isAnsiEnabled() ? ANSI_RED : "")
	   << "[-] Loss: " << std::setw(7) << fmtKw(engine.getTotalLosses())
	   << (isAnsiEnabled() ? ANSI_RESET : "") << " "
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
			(i == angPos ? (isAnsiEnabled() ? "\033[1m*\033[0m" : "*") : "-");
	angSpark += "]";
	h3 << "  " << angSpark;

	out << bL << fixLen(f3.str(), 15) << bM << fixLen(p3.str(), 34) << bM
		<< fixLen(h3.str(), 24) << bR;

	std::ostringstream f4, p4, h4;
	std::string fSpark = "[-";
	int mappedPos = static_cast<int>(pos * 8.0 / barWidth);
	for (int i = 0; i < 8; ++i)
		fSpark += (i == mappedPos ? (isAnsiEnabled() ? "\033[1m*\033[0m" : "*")
								  : "-");
	fSpark += "]";
	f4 << "  " << fSpark;
	double imb = engine.getTotalGeneration() - engine.getTotalLoad() -
				 engine.getTotalLosses();
	const char *imbColor = (std::abs(imb) > 5.0) ? ANSI_YELLOW : ANSI_RESET;
	p4 << "  " << (isAnsiEnabled() ? imbColor : "")
	   << "[=] Imb:  " << std::setw(7) << fmtKw(imb)
	   << (isAnsiEnabled() ? ANSI_RESET : "");
	std::string lineInfo = nameWorstLine.empty() ? "None" : nameWorstLine;
	if (lineInfo.size() > 10)
		lineInfo = lineInfo.substr(0, 10);
	h4 << "  Line: " << lineInfo;
	out << bL << fixLen(f4.str(), 15) << bM << fixLen(p4.str(), 34) << bM
		<< fixLen(h4.str(), 24) << bR;

	std::ostringstream f5, p5, h5;
	f5 << " ";
	p5 << " ";
	h5 << "  " << (isAnsiEnabled() ? lColor : "") << std::fixed
	   << std::setprecision(1) << worstLinePct << "% "
	   << (isAnsiEnabled() ? ANSI_RESET : "") << drawBar(worstLinePct, 100.0)
	   << (worstLinePct > 100.0 ? "!" : "");
	out << bL << fixLen(f5.str(), 15) << bM << fixLen(p5.str(), 34) << bM
		<< fixLen(h5.str(), 24) << bR;
	out << cCol << "  " << midBorder << "\n" << cRes;

	std::string lastEvent = engine.getLastEvent();
	if (lastEvent.empty())
		lastEvent = "Simulation nominal.";
	if (static_cast<int>(lastEvent.size()) > DASHBOARD_INNER_WIDTH - 6)
		lastEvent = lastEvent.substr(0, DASHBOARD_INNER_WIDTH - 9) + "...";
	out << bL << fixLen("  Log:", DASHBOARD_INNER_WIDTH) << bR;
	out << bL << fixLen("  " + lastEvent, DASHBOARD_INNER_WIDTH) << bR;

	out << cCol << "  " << topBorder << "\n" << cRes;
	std::cout << out.str() << std::flush;
}

} // namespace GLStation::UI

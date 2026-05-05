#include "grid/Breaker.hpp"
#include "grid/Generator.hpp"
#include "grid/GridComponent.hpp"
#include "grid/Line.hpp"
#include "grid/Load.hpp"
#include "grid/Node.hpp"
#include "grid/Shunt.hpp"
#include "grid/Transformer.hpp"
#include "io/handlers/CSVHandler.hpp"
#include "log/Logger.hpp"
#include "sim/Engine.hpp"
#include "sim/PowerSolver.hpp"
#include "util/Random.hpp"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace GLStation::Simulation {

struct GridLoadingContext {
	std::map<std::string, std::shared_ptr<Grid::Substation>> substations;
	std::map<std::string, Grid::Node *> nodes;
};

void Engine::initialise() {
	Grid::GridComponent::resetIdCounter();
	PowerSolver::invalidateYBus();
	Util::Random::init(12345);
	m_systemFrequency = m_nominalHz;
	m_lastFreqHz = m_nominalHz;
	m_agcIntegralMw = 0.0;
	m_rocofHzPerS = 0.0;
	m_substations.clear();
	m_currentTick = 0;
	m_simTime = std::chrono::milliseconds{0};
	Log::Diagnostics::clear();
	m_pendingTrips.clear();
	m_overloadStartTick.clear();
	m_recloseAtTick.clear();
	m_recloseCooldownUntil.clear();
	m_recloseLockout.clear();
	m_frequencyNadir = m_nominalHz;
	m_frequencyNadirLifetime = m_nominalHz;
	m_maxObservedLineLoadingPercent = 0.0;
	m_maxLineLoadingLifetime = 0.0;
	m_activeShedLoadKw = 0.0;
	m_reserveMarginKw = 0.0;

	initialiseUflsStages();
	for (auto &s : m_uflsStages) {
		s.triggered = false;
		s.armPending = false;
		s.armAtTick = 0;
	}

	if (std::filesystem::exists("grid.csv")) {
		loadGrid("grid.csv");
	} else {
		createDemoGrid();
		configureDemoProfiles();
	}
	Log::Logger::info("Grid initialised.");
}

static std::string genProfileToStr(Grid::GeneratorProfile p) {
	switch (p) {
	case Grid::GeneratorProfile::Thermal:
		return "Thermal";
	case Grid::GeneratorProfile::Hydro:
		return "Hydro";
	case Grid::GeneratorProfile::Wind:
		return "Wind";
	case Grid::GeneratorProfile::Solar:
		return "Solar";
	default:
		return "Manual";
	}
}

static Grid::GeneratorProfile strToGenProfile(const std::string &s) {
	if (s == "Thermal")
		return Grid::GeneratorProfile::Thermal;
	if (s == "Hydro")
		return Grid::GeneratorProfile::Hydro;
	if (s == "Wind")
		return Grid::GeneratorProfile::Wind;
	if (s == "Solar")
		return Grid::GeneratorProfile::Solar;
	return Grid::GeneratorProfile::Manual;
}

static std::string loadProfileToStr(Grid::LoadProfile p) {
	switch (p) {
	case Grid::LoadProfile::Residential:
		return "Residential";
	case Grid::LoadProfile::Commercial:
		return "Commercial";
	case Grid::LoadProfile::Industrial:
		return "Industrial";
	default:
		return "Flat";
	}
}

static Grid::LoadProfile strToLoadProfile(const std::string &s) {
	if (s == "Residential")
		return Grid::LoadProfile::Residential;
	if (s == "Commercial")
		return Grid::LoadProfile::Commercial;
	if (s == "Industrial")
		return Grid::LoadProfile::Industrial;
	return Grid::LoadProfile::Flat;
}

static std::string d2s(Core::f64 v) { return std::to_string(v); }

Log::Status Engine::saveGrid(const std::string &filename) const {
	std::ofstream f(filename);
	if (!f.is_open()) {
		return {Log::ErrorCode::Unknown,
				"Failed to open file for saving: " + filename};
	}

	for (const auto &sub : m_substations) {
		IO::CSVHandler::writeRow(f, {"SUBSTATION", sub->getName()});

		for (const auto &comp : sub->getComponents()) {
			if (auto n = dynamic_cast<Grid::Node *>(comp.get())) {
				IO::CSVHandler::writeRow(
					f, {"NODE", n->getName(), d2s(n->getBaseVoltage())});
			}
		}

		for (const auto &comp : sub->getComponents()) {
			if (dynamic_cast<Grid::Node *>(comp.get()))
				continue;

			if (auto l = dynamic_cast<Grid::Line *>(comp.get())) {
				IO::CSVHandler::writeRow(
					f, {"LINE", l->getName(), l->getFromNode()->getName(),
						l->getToNode()->getName(), d2s(l->getResistance()),
						d2s(l->getReactance()),
						d2s(l->getLineChargingSusceptancePu()),
						d2s(l->getCurrentLimit())});
			} else if (auto ld = dynamic_cast<Grid::Load *>(comp.get())) {
				IO::CSVHandler::writeRow(
					f,
					{"LOAD", ld->getName(), ld->getConnectedNode()->getName(),
					 d2s(ld->getMaxPower()), d2s(ld->getPowerFactor()),
					 loadProfileToStr(ld->getProfile())});
			} else if (auto g = dynamic_cast<Grid::Generator *>(comp.get())) {
				std::string mode = "PQ";
				if (g->getMode() == Grid::GeneratorMode::Slack)
					mode = "Slack";
				else if (g->getMode() == Grid::GeneratorMode::PV)
					mode = "PV";
				IO::CSVHandler::writeRow(
					f, {"GENERATOR", g->getName(),
						g->getConnectedNode()->getName(), mode,
						d2s(g->getTargetP()), d2s(g->getTargetV()),
						d2s(g->getMinQ()), d2s(g->getMaxQ()),
						d2s(g->getMinPower()), d2s(g->getMaxPower()),
						genProfileToStr(g->getProfile())});
			} else if (auto t = dynamic_cast<Grid::Transformer *>(comp.get())) {
				IO::CSVHandler::writeRow(
					f,
					{"TRANSFORMER", t->getName(),
					 t->getPrimaryNode()->getName(),
					 t->getSecondaryNode()->getName(), d2s(t->getResistance()),
					 d2s(t->getReactance()), d2s(t->getTap()),
					 d2s(t->getPhaseShiftDeg()), d2s(t->getCurrentLimit())});
			} else if (auto s = dynamic_cast<Grid::Shunt *>(comp.get())) {
				IO::CSVHandler::writeRow(
					f, {"SHUNT", s->getName(), s->getNode()->getName(),
						d2s(s->getGPu()), d2s(s->getBPu())});
			} else if (auto b = dynamic_cast<Grid::Breaker *>(comp.get())) {
				IO::CSVHandler::writeRow(
					f, {"BREAKER", b->getName(), b->getFromNode()->getName(),
						b->getToNode()->getName(), b->isOpen() ? "1" : "0"});
			}
		}
		f << "\n";
	}
	Log::Logger::info("Saved grid to " + filename);
	return {Log::ErrorCode::Success, ""};
}

Log::Status Engine::loadGrid(const std::string &filename) {
	std::ifstream file(filename);
	if (!file.is_open()) {
		return {Log::ErrorCode::Unknown,
				"Failed to open file for loading: " + filename};
	}

	clearSubstations();
	PowerSolver::invalidateYBus();
	Grid::GridComponent::resetIdCounter();

	GridLoadingContext ctx;
	std::shared_ptr<Grid::Substation> activeSub;
	auto ensureSub = [&](const std::string &name) {
		if (!activeSub || activeSub->getName() != name) {
			if (ctx.substations.count(name)) {
				activeSub = ctx.substations[name];
			} else {
				activeSub = std::make_shared<Grid::Substation>(name);
				ctx.substations[name] = activeSub;
				m_substations.push_back(activeSub);
			}
		}
	};

	std::string line;
	int lineNum = 0;
	int skipped = 0;
	while (std::getline(file, line)) {
		++lineNum;
		if (IO::CSVHandler::isCommentOrEmpty(line))
			continue;

		std::vector<std::string> f;
		try {
			f = IO::CSVHandler::parseCSVLine(line);
		} catch (...) {
			Log::Logger::warn("Line " + std::to_string(lineNum) +
							  " contains malformed CSV");
			++skipped;
			continue;
		}
		if (f.empty())
			continue;

		std::string type = f[0];
		for (auto &c : type)
			c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

		try {
			if (type == "SUBSTATION") {
				std::string subName = f.size() > 1
										  ? IO::CSVHandler::trimField(f[1])
										  : "Imported Substation";
				ensureSub(subName);
				continue;
			}

			ensureSub("Imported Substation");

			if (type == "NODE" && f.size() >= 3) {
				std::string name = f[1];
				Core::f64 kv = std::stod(f[2]);
				auto node = std::make_shared<Grid::Node>(name, kv);
				ctx.nodes[name] = node.get();
				activeSub->addComponent(node);

			} else if (type == "LINE" && f.size() >= 6) {
				std::string name = f[1], from = f[2], to = f[3];
				Core::f64 r = std::stod(f[4]), x = std::stod(f[5]);
				Core::f64 bchg = f.size() >= 7 ? std::stod(f[6]) : 0.0;
				Core::f64 limit = f.size() >= 8 ? std::stod(f[7]) : 0.0;
				if (ctx.nodes.count(from) && ctx.nodes.count(to)) {
					auto lineObj = std::make_shared<Grid::Line>(
						name, ctx.nodes[from], ctx.nodes[to], r, x, bchg);
					if (limit > 0)
						lineObj->setCurrentLimit(limit);
					activeSub->addComponent(lineObj);
				}

			} else if (type == "LOAD" && f.size() >= 4) {
				std::string name = f[1], node = f[2];
				Core::f64 kw = std::stod(f[3]);
				Core::f64 pf = f.size() >= 5 ? std::stod(f[4]) : 0.95;
				Grid::LoadProfile prof = f.size() >= 6
											 ? strToLoadProfile(f[5])
											 : Grid::LoadProfile::Flat;
				if (ctx.nodes.count(node)) {
					auto loadObj =
						std::make_shared<Grid::Load>(name, ctx.nodes[node], kw);
					loadObj->setPowerFactor(pf);
					loadObj->setProfile(prof);
					activeSub->addComponent(loadObj);
				}

			} else if ((type == "GEN" || type == "GENERATOR") &&
					   f.size() >= 6) {
				std::string name = f[1], node = f[2], modeStr = f[3];
				Core::f64 p = std::stod(f[4]), v = std::stod(f[5]);
				Core::f64 qmin = f.size() >= 7 ? std::stod(f[6]) : -1e9;
				Core::f64 qmax = f.size() >= 8 ? std::stod(f[7]) : 1e9;
				Core::f64 pmin = f.size() >= 9 ? std::stod(f[8]) : 0.0;
				Core::f64 pmax = f.size() >= 10 ? std::stod(f[9]) : p;
				Grid::GeneratorProfile prof =
					f.size() >= 11 ? strToGenProfile(f[10])
								   : Grid::GeneratorProfile::Manual;
				for (auto &c : modeStr)
					c = static_cast<char>(
						std::tolower(static_cast<unsigned char>(c)));
				if (ctx.nodes.count(node)) {
					Grid::GeneratorMode mode = Grid::GeneratorMode::PQ;
					if (modeStr == "slack")
						mode = Grid::GeneratorMode::Slack;
					else if (modeStr == "pv")
						mode = Grid::GeneratorMode::PV;
					auto gen = std::make_shared<Grid::Generator>(
						name, ctx.nodes[node], mode);
					gen->setTargetP(p);
					gen->setTargetV(v);
					gen->setQLimits(qmin, qmax);
					gen->setPowerBounds(pmin, pmax);
					gen->setProfile(prof);
					activeSub->addComponent(gen);
				}

			} else if ((type == "TRAFO" || type == "TRANSFORMER") &&
					   f.size() >= 7) {
				std::string name = f[1], pri = f[2], sec = f[3];
				Core::f64 r = std::stod(f[4]), x = std::stod(f[5]),
						  tap = std::stod(f[6]);
				Core::f64 ph = f.size() >= 8 ? std::stod(f[7]) : 0.0;
				Core::f64 limit = f.size() >= 9 ? std::stod(f[8]) : 0.0;
				if (ctx.nodes.count(pri) && ctx.nodes.count(sec)) {
					auto trafo = std::make_shared<Grid::Transformer>(
						name, ctx.nodes[pri], ctx.nodes[sec], r, x, tap);
					trafo->setPhaseShiftDeg(ph);
					if (limit > 0)
						trafo->setCurrentLimit(limit);
					activeSub->addComponent(trafo);
				}

			} else if (type == "SHUNT" && f.size() >= 4) {
				std::string name = f[1], node = f[2];
				Core::f64 gpu = std::stod(f[3]);
				Core::f64 bpu = f.size() >= 5 ? std::stod(f[4]) : 0.0;
				if (ctx.nodes.count(node)) {
					auto sh = std::make_shared<Grid::Shunt>(
						name, ctx.nodes[node], gpu, bpu);
					activeSub->addComponent(sh);
				}

			} else if (type == "BREAKER" && f.size() >= 4) {
				std::string name = f[1], from = f[2], to = f[3];
				bool isOpen = f.size() >= 5 && f[4] == "1";
				if (ctx.nodes.count(from) && ctx.nodes.count(to)) {
					auto brk = std::make_shared<Grid::Breaker>(
						name, ctx.nodes[from], ctx.nodes[to]);
					brk->setOpen(isOpen);
					activeSub->addComponent(brk);
				}
			}
		} catch (const std::exception &e) {
			Log::Logger::warn("Line " + std::to_string(lineNum) + ": " +
							  e.what() + "skipped");
			++skipped;
		}
	}

	if (skipped > 0)
		Log::Logger::warn(std::to_string(skipped) + " lines skipped.");
	Log::Logger::info("Loaded grid from " + filename);
	return {Log::ErrorCode::Success, ""};
}

} // namespace GLStation::Simulation

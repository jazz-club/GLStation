#include "grid/Breaker.hpp"
#include "grid/Generator.hpp"
#include "grid/GridComponent.hpp"
#include "grid/Line.hpp"
#include "grid/Load.hpp"
#include "grid/Node.hpp"
#include "grid/Shunt.hpp"
#include "grid/Transformer.hpp"
#include "io/handlers/CSVHandler.hpp"
#include "sim/Engine.hpp"
#include "sim/PowerSolver.hpp"
#include "util/Random.hpp"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
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

	{
		std::error_code ec;
		std::filesystem::remove("gls.csv", ec);
	}

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
	std::cout << "Grid initialised." << std::endl;
}

void Engine::saveGrid(const std::string &filename) const {
	std::ofstream f(filename);
	if (!f.is_open()) {
		std::cout << "Failed to open file for saving: " << filename
				  << std::endl;
		return;
	}

	for (const auto &sub : m_substations) {
		f << "SUBSTATION," << sub->getName() << "\n";
		for (const auto &comp : sub->getComponents()) {
			if (auto n = dynamic_cast<Grid::Node *>(comp.get())) {
				f << "NODE," << n->getName() << "," << n->getBaseVoltage()
				  << "\n";
			}
		}
	}

	for (const auto &sub : m_substations) {
		f << "SUBSTATION," << sub->getName() << "\n";
		for (const auto &comp : sub->getComponents()) {
			if (dynamic_cast<Grid::Node *>(comp.get()))
				continue;

			if (auto l = dynamic_cast<Grid::Line *>(comp.get())) {
				f << "LINE," << l->getName() << ","
				  << l->getFromNode()->getName() << ","
				  << l->getToNode()->getName() << "," << l->getResistance()
				  << "," << l->getReactance() << ","
				  << l->getLineChargingSusceptancePu() << "\n";
			} else if (auto ld = dynamic_cast<Grid::Load *>(comp.get())) {
				f << "LOAD," << ld->getName() << ","
				  << ld->getConnectedNode()->getName() << ","
				  << ld->getMaxPower() << "\n";
			} else if (auto g = dynamic_cast<Grid::Generator *>(comp.get())) {
				std::string mode = "PQ";
				if (g->getMode() == Grid::GeneratorMode::Slack)
					mode = "Slack";
				else if (g->getMode() == Grid::GeneratorMode::PV)
					mode = "PV";
				f << "GENERATOR," << g->getName() << ","
				  << g->getConnectedNode()->getName() << "," << mode << ","
				  << g->getTargetP() << "," << g->getTargetV() << ","
				  << g->getMinQ() << "," << g->getMaxQ() << "\n";
			} else if (auto t = dynamic_cast<Grid::Transformer *>(comp.get())) {
				f << "TRANSFORMER," << t->getName() << ","
				  << t->getPrimaryNode()->getName() << ","
				  << t->getSecondaryNode()->getName() << ","
				  << t->getResistance() << "," << t->getReactance() << ","
				  << t->getTap() << "," << t->getPhaseShiftDeg() << "\n";
			} else if (auto s = dynamic_cast<Grid::Shunt *>(comp.get())) {
				f << "SHUNT," << s->getName() << "," << s->getNode()->getName()
				  << "," << s->getGPu() << "," << s->getBPu() << "\n";
			} else if (auto b = dynamic_cast<Grid::Breaker *>(comp.get())) {
				f << "BREAKER," << b->getName() << ","
				  << b->getFromNode()->getName() << ","
				  << b->getToNode()->getName() << "\n";
			}
		}
	}
	std::cout << "Saved grid to " << filename << std::endl;
}

void Engine::loadGrid(const std::string &filename) {
	std::ifstream file(filename);
	if (!file.is_open()) {
		std::cout << "Failed to open file for loading: " << filename
				  << std::endl;
		return;
	}

	clearSubstations();
	PowerSolver::invalidateYBus();
	Grid::GridComponent::resetIdCounter();

	GridLoadingContext ctx;
	std::shared_ptr<Grid::Substation> defaultSub;
	auto ensureSub = [&](const std::string &name) {
		if (!defaultSub || defaultSub->getName() != name) {
			if (ctx.substations.count(name)) {
				defaultSub = ctx.substations[name];
			} else {
				defaultSub = std::make_shared<Grid::Substation>(name);
				ctx.substations[name] = defaultSub;
				m_substations.push_back(defaultSub);
			}
		}
	};

	std::string line;
	while (std::getline(file, line)) {
		if (line.empty())
			continue;
		std::vector<std::string> f = Util::CSVHandler::parseCSVLine(line);
		if (f.empty())
			continue;
		std::string type = f[0];
		for (auto &c : type)
			c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

		if (type == "SUBSTATION") {
			std::string subName = f.size() > 1
									  ? Util::CSVHandler::trimField(f[1])
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
			defaultSub->addComponent(node);
		} else if (type == "LINE" && f.size() >= 6) {
			std::string name = f[1], from = f[2], to = f[3];
			Core::f64 r = std::stod(f[4]), x = std::stod(f[5]);
			Core::f64 bchg = 0.0;
			if (f.size() >= 7)
				bchg = std::stod(f[6]);
			if (ctx.nodes.count(from) && ctx.nodes.count(to)) {
				auto lineObj = std::make_shared<Grid::Line>(
					name, ctx.nodes[from], ctx.nodes[to], r, x, bchg);
				defaultSub->addComponent(lineObj);
			}
		} else if (type == "LOAD" && f.size() >= 4) {
			std::string name = f[1], node = f[2];
			Core::f64 kw = std::stod(f[3]);
			if (ctx.nodes.count(node)) {
				auto loadObj =
					std::make_shared<Grid::Load>(name, ctx.nodes[node], kw);
				defaultSub->addComponent(loadObj);
			}
		} else if ((type == "GEN" || type == "GENERATOR") && f.size() >= 8) {
			std::string name = f[1], node = f[2], modeStr = f[3];
			Core::f64 p = std::stod(f[4]), v = std::stod(f[5]),
					  qmin = std::stod(f[6]), qmax = std::stod(f[7]);
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
				defaultSub->addComponent(gen);
			}
		} else if ((type == "TRAFO" || type == "TRANSFORMER") &&
				   f.size() >= 7) {
			std::string name = f[1], pri = f[2], sec = f[3];
			Core::f64 r = std::stod(f[4]), x = std::stod(f[5]),
					  tap = std::stod(f[6]);
			Core::f64 ph = 0.0;
			if (f.size() >= 8)
				ph = std::stod(f[7]);
			if (ctx.nodes.count(pri) && ctx.nodes.count(sec)) {
				auto trafo = std::make_shared<Grid::Transformer>(
					name, ctx.nodes[pri], ctx.nodes[sec], r, x, tap);
				trafo->setPhaseShiftDeg(ph);
				defaultSub->addComponent(trafo);
			}
		} else if (type == "SHUNT" && f.size() >= 4) {
			std::string name = f[1], node = f[2];
			Core::f64 gpu = std::stod(f[3]);
			Core::f64 bpu = f.size() >= 5 ? std::stod(f[4]) : 0.0;
			if (ctx.nodes.count(node)) {
				auto sh = std::make_shared<Grid::Shunt>(name, ctx.nodes[node],
														gpu, bpu);
				defaultSub->addComponent(sh);
			}
		} else if (type == "BREAKER" && f.size() >= 4) {
			std::string name = f[1], from = f[2], to = f[3];
			if (ctx.nodes.count(from) && ctx.nodes.count(to)) {
				auto brk = std::make_shared<Grid::Breaker>(
					name, ctx.nodes[from], ctx.nodes[to]);
				defaultSub->addComponent(brk);
			}
		}
	}
	std::cout << "Loaded grid from " << filename << std::endl;
}

} // namespace GLStation::Simulation

#include "grid/Breaker.hpp"
#include "grid/Generator.hpp"
#include "grid/GridComponent.hpp"
#include "grid/Line.hpp"
#include "grid/Load.hpp"
#include "grid/Node.hpp"
#include "grid/Shunt.hpp"
#include "grid/Transformer.hpp"
#include "io/CSVHandler.hpp"
#include "sim/Engine.hpp"
#include "sim/PowerSolver.hpp"
#include "util/Random.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace GLStation::Simulation {

static SimTickState g_simTickState{};

SimTickState &Engine::simTickState() { return g_simTickState; }

/*
		load shreading thresholds
		erik you probably understand 
*/

/*
		sources;
		https://www.aemo.com.au/energy-systems/electricity/wholesale-electricity-market-wem/system-operations/under-frequency-load-shedding
		https://www.sciencedirect.com/science/article/pii/S1364032123001508
*/

struct GridLoadingContext {
	std::map<std::string, std::shared_ptr<Grid::Substation>> substations;
	std::map<std::string, Grid::Node *> nodes;
};

Engine::Engine()
	: m_currentTick(0), m_simTime(std::chrono::milliseconds{0}),
	  m_simStep(std::chrono::milliseconds{1}), m_nominalHz(50.0),
	  m_lastFreqHz(50.0), m_agcIntegralMw(0.0), m_rocofHzPerS(0.0),
	  m_systemFrequency(50.0), m_totalGeneration(0), m_totalLoad(0),
	  m_totalLosses(0), m_frequencyNadir(50.0),
	  m_maxObservedLineLoadingPercent(0.0), m_activeShedLoadKw(0.0),
	  m_reserveMarginKw(0.0) {}

void Engine::pushSimTickState() {
	g_simTickState.simTime = m_simTime;
	g_simTickState.step = m_currentTick;
	g_simTickState.nominalHz = m_nominalHz;
	g_simTickState.systemHz = m_systemFrequency;
	g_simTickState.dtSeconds =
		std::chrono::duration<Core::f64>(m_simStep).count() / 1000.0;
}

void Engine::loadUflsFile() {
	m_uflsStages.clear();
	std::ifstream uf("ufls.csv");
	if (!uf.is_open()) {
		m_uflsStages.push_back({49.0, 0.10, 0, 1e9, false, false, 0});
		m_uflsStages.push_back({48.7, 0.15, 0, 1e9, false, false, 0});
		m_uflsStages.push_back({48.4, 0.20, 0, 1e9, false, false, 0});
		m_uflsStages.push_back({48.0, 0.30, 0, 1e9, false, false, 0});
		return;
	}
	std::string row;
	while (std::getline(uf, row)) {
		if (row.empty())
			continue;
		auto f = Util::CSVHandler::parseCSVLine(row);
		if (f.size() < 3)
			continue;
		try {
			Core::f64 fh = std::stod(f[0]);
			Core::f64 fr = std::stod(f[1]);
			Core::u64 dly = static_cast<Core::u64>(std::stoull(f[2]));
			Core::f64 rc = f.size() > 3 ? std::stod(f[3]) : 1e9;
			m_uflsStages.push_back({fh, fr, dly, rc, false, false, 0});
		} catch (...) {
		}
	}
	if (m_uflsStages.empty()) {
		m_uflsStages.push_back({49.0, 0.10, 0, 1e9, false, false, 0});
		m_uflsStages.push_back({48.7, 0.15, 0, 1e9, false, false, 0});
		m_uflsStages.push_back({48.4, 0.20, 0, 1e9, false, false, 0});
		m_uflsStages.push_back({48.0, 0.30, 0, 1e9, false, false, 0});
	}
}

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
	m_eventLog.clear();
	m_pendingTrips.clear();
	m_overloadStartTick.clear();
	m_recloseAtTick.clear();
	m_recloseCooldownUntil.clear();
	m_frequencyNadir = m_nominalHz;
	m_maxObservedLineLoadingPercent = 0.0;
	m_activeShedLoadKw = 0.0;
	m_reserveMarginKw = 0.0;

	loadUflsFile();
	for (auto &s : m_uflsStages) {
		s.triggered = false;
		s.armPending = false;
		s.armAtTick = 0;
	}

	if (std::filesystem::exists("grid.csv")) {
		std::ifstream file("grid.csv");
		if (file.is_open()) {
			GridLoadingContext ctx;
			std::shared_ptr<Grid::Substation> defaultSub;
			auto ensureSub = [&](const std::string &name) {
				if (!defaultSub) {
					defaultSub = std::make_shared<Grid::Substation>(name);
					ctx.substations["default"] = defaultSub;
					m_substations.push_back(defaultSub);
				}
			};

			std::string line;
			while (std::getline(file, line)) {
				if (line.empty())
					continue;
				std::vector<std::string> f =
					Util::CSVHandler::parseCSVLine(line);
				if (f.empty())
					continue;
				std::string type = f[0];
				for (auto &c : type)
					c = static_cast<char>(
						std::toupper(static_cast<unsigned char>(c)));

				if (type == "SUBSTATION") {
					std::string subName =
						f.size() > 1 ? Util::CSVHandler::trimField(f[1]) : "";
					if (!subName.empty())
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
						auto loadObj = std::make_shared<Grid::Load>(
							name, ctx.nodes[node], kw);
						defaultSub->addComponent(loadObj);
					}
				} else if ((type == "GEN" || type == "GENERATOR") &&
						   f.size() >= 8) {
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
						auto sh = std::make_shared<Grid::Shunt>(
							name, ctx.nodes[node], gpu, bpu);
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
		}
	} else {
		createDemoGrid();
		configureDemoProfiles();
	}
	std::cout << "Grid initialised." << std::endl;
}

/*
		Fallback grid, but uh, this is sorta ass and the point at which im giving up on this project because theres a bridge between the real world 
		and demos we cant really get over at the moment
*/
void Engine::createDemoGrid() {
	auto north = std::make_shared<Grid::Substation>("North220kV");
	auto central = std::make_shared<Grid::Substation>("Central110kV");
	auto south = std::make_shared<Grid::Substation>("South110kV");

	auto nGrid = std::make_shared<Grid::Node>("N_Grid", 220.0);
	auto nEast = std::make_shared<Grid::Node>("N_East", 220.0);
	auto cHub = std::make_shared<Grid::Node>("C_Hub", 110.0);
	auto cWest = std::make_shared<Grid::Node>("C_West", 110.0);
	auto sHub = std::make_shared<Grid::Node>("S_Hub", 110.0);
	auto sPort = std::make_shared<Grid::Node>("S_Port", 110.0);

	auto slack = std::make_shared<Grid::Generator>("NorthThermal", nGrid.get(),
												   Grid::GeneratorMode::Slack);
	slack->setTargetV(1.03);
	slack->setPowerBounds(30000.0, 160000.0);

	auto hydro = std::make_shared<Grid::Generator>("RiverHydro", cHub.get(),
												   Grid::GeneratorMode::PV);
	hydro->setTargetP(45000.0);
	hydro->setTargetV(1.01);
	hydro->setPowerBounds(20000.0, 70000.0);

	auto wind = std::make_shared<Grid::Generator>("CoastalWind", sPort.get(),
												  Grid::GeneratorMode::PV);
	wind->setTargetP(30000.0);
	wind->setTargetV(1.00);
	wind->setPowerBounds(5000.0, 50000.0);

	auto solar = std::make_shared<Grid::Generator>("CentralSolar", cWest.get(),
												   Grid::GeneratorMode::PV);
	solar->setTargetP(22000.0);
	solar->setTargetV(1.01);
	solar->setPowerBounds(2000.0, 32000.0);

	auto lNE = std::make_shared<Grid::Line>("L_NE_220", nGrid.get(),
											nEast.get(), 1.2, 7.0);
	auto lNC = std::make_shared<Grid::Line>("L_NC_220_110", nEast.get(),
											cHub.get(), 1.8, 8.5);
	auto lCS = std::make_shared<Grid::Line>("L_CS_110", cHub.get(), sHub.get(),
											2.3, 10.5);
	auto lCW = std::make_shared<Grid::Line>("L_CW_110", cHub.get(), cWest.get(),
											1.9, 8.0);
	auto lSP = std::make_shared<Grid::Line>("L_SP_110", sHub.get(), sPort.get(),
											1.5, 7.0);
	auto tie = std::make_shared<Grid::Line>("L_TIE_110", cWest.get(),
											sHub.get(), 2.8, 11.0);

	lNE->setCurrentLimit(1200.0);
	lNC->setCurrentLimit(900.0);
	lCS->setCurrentLimit(750.0);
	lCW->setCurrentLimit(650.0);
	lSP->setCurrentLimit(600.0);
	tie->setCurrentLimit(450.0);

	auto trNorth = std::make_shared<Grid::Transformer>(
		"T_North220_110", nEast.get(), cHub.get(), 0.8, 6.5, 1.00);
	auto trSouth = std::make_shared<Grid::Transformer>(
		"T_South220_110", nGrid.get(), sHub.get(), 0.9, 6.8, 0.98);
	trNorth->setCurrentLimit(1000.0);
	trSouth->setCurrentLimit(950.0);

	auto brTie =
		std::make_shared<Grid::Breaker>("BR_TIE", cWest.get(), sHub.get());
	auto brCS =
		std::make_shared<Grid::Breaker>("BR_CS", cHub.get(), sHub.get());

	auto cRes =
		std::make_shared<Grid::Load>("MetroResidential", cHub.get(), 32000.0);
	auto cCom =
		std::make_shared<Grid::Load>("MetroCommercial", cWest.get(), 28000.0);
	auto sRes =
		std::make_shared<Grid::Load>("SouthResidential", sHub.get(), 26000.0);
	auto sInd =
		std::make_shared<Grid::Load>("PortIndustrial", sPort.get(), 78000.0);

	north->addComponent(nGrid);
	north->addComponent(nEast);
	north->addComponent(slack);
	north->addComponent(lNE);
	north->addComponent(lNC);
	north->addComponent(trNorth);
	north->addComponent(trSouth);

	central->addComponent(cHub);
	central->addComponent(cWest);
	central->addComponent(hydro);
	central->addComponent(solar);
	central->addComponent(lCS);
	central->addComponent(lCW);
	central->addComponent(tie);
	central->addComponent(brTie);
	central->addComponent(brCS);
	central->addComponent(cRes);
	central->addComponent(cCom);

	south->addComponent(sHub);
	south->addComponent(sPort);
	south->addComponent(wind);
	south->addComponent(lSP);
	south->addComponent(sRes);
	south->addComponent(sInd);

	m_substations.push_back(north);
	m_substations.push_back(central);
	m_substations.push_back(south);
	logEvent("Demo grid loaded: 3 substations, 6 lines, 2 transformers.");
}

/*
		tick manager, need to add debug lines per component
		most of the logic here is self explanatory because of the component names
		slack and loading balacing and AGC trigger at the end of every tick, probably not the most optimised solution
*/
void Engine::tick() {
	m_currentTick++;
	m_simTime += m_simStep;
	pushSimTickState();
	m_scenarioManager.update(m_currentTick);

	m_totalLoad = 0.0;
	for (auto &sub : m_substations) {
		sub->tick(m_currentTick);
		for (auto &comp : sub->getComponents()) {
			if (auto load = dynamic_cast<Grid::Load *>(comp.get())) {
				m_totalLoad += load->getCurrentConsumption();
			}
		}
	}

	for (auto &sub : m_substations) {
		for (auto &comp : sub->getComponents()) {
			if (auto gen = dynamic_cast<Grid::Generator *>(comp.get())) {
				gen->applyDroopResponse(m_systemFrequency, m_nominalHz);
				gen->stepGovernor(g_simTickState.dtSeconds);
			}
		}
	}

	PowerSolver::solve(m_substations, SolverSettings());

	for (auto &sub : m_substations) {
		for (auto &comp : sub->getComponents()) {
			if (auto line = dynamic_cast<Grid::Line *>(comp.get()))
				line->tick(m_currentTick);
			if (auto trafo = dynamic_cast<Grid::Transformer *>(comp.get()))
				trafo->tick(m_currentTick);
		}
	}

	m_totalGeneration = 0.0;
	m_totalLosses = 0.0;
	for (auto &sub : m_substations) {
		for (auto &comp : sub->getComponents()) {
			if (auto gen = dynamic_cast<Grid::Generator *>(comp.get()))
				m_totalGeneration += gen->getActualP();
			if (auto line = dynamic_cast<Grid::Line *>(comp.get()))
				m_totalLosses += line->getLosses();
			if (auto trafo = dynamic_cast<Grid::Transformer *>(comp.get()))
				m_totalLosses += trafo->getLosses();
		}
	}

	updateKpis();
	Core::f64 dt = std::chrono::duration<Core::f64>(m_simStep).count() / 1000.0;
	if (dt > 1e-9)
		m_rocofHzPerS = (m_systemFrequency - m_lastFreqHz) / dt;
	m_lastFreqHz = m_systemFrequency;
	updateFrequencyDynamics();
	applyAGC();
	for (auto &sub : m_substations) {
		for (auto &comp : sub->getComponents()) {
			if (auto gen = dynamic_cast<Grid::Generator *>(comp.get()))
				gen->stepExciter(g_simTickState.dtSeconds);
		}
	}
	processProtectionRelays();
	updateKpis();
}

/*
		hardcoded to 50hz still
		df/dt ∝ (P_gen - P_load - P_loss) / (2HSbase), range is 49-51(?) 
*/
void Engine::updateFrequencyDynamics() {
	Core::f64 Hsum = 0.0;
	for (auto &sub : m_substations) {
		for (auto &comp : sub->getComponents()) {
			if (auto gen = dynamic_cast<Grid::Generator *>(comp.get()))
				Hsum += std::max(0.1, gen->getInertiaH());
		}
	}
	if (Hsum < 1e-6)
		Hsum = 5.0;
	const Core::f64 Sbase = 100000.0;
	const Core::f64 D = 0.08;
	Core::f64 dt = std::chrono::duration<Core::f64>(m_simStep).count() / 1000.0;

	Core::f64 imbalance = m_totalGeneration - m_totalLoad - m_totalLosses;
	Core::f64 df_dt = (imbalance / (2.0 * Hsum * Sbase)) * m_nominalHz -
					  D * (m_systemFrequency - m_nominalHz);
	m_systemFrequency += df_dt * dt;
	m_systemFrequency =
		std::clamp(m_systemFrequency, m_nominalHz - 3.0, m_nominalHz + 2.0);
}

/*
		-df * gain for slope across gens
		i might be misunderstanding the underlying logic 
		https://www.academia.edu/32675657/Automatic_Gain_Control_AGC_in_Receivers
*/
void Engine::applyAGC() {
	Core::f64 dt = std::chrono::duration<Core::f64>(m_simStep).count() / 1000.0;
	Core::f64 ace = -(m_systemFrequency - m_nominalHz);
	if (std::abs(ace) < 0.005)
		return;
	m_agcIntegralMw += ace * dt;
	m_agcIntegralMw = std::clamp(m_agcIntegralMw, -5000.0, 5000.0);
	Core::f64 correction =
		std::clamp(-ace * 120.0 - m_agcIntegralMw * 4.0, -180.0, 180.0);
	for (auto &sub : m_substations) {
		for (auto &comp : sub->getComponents()) {
			if (auto gen = dynamic_cast<Grid::Generator *>(comp.get())) {
				if (gen->getMode() != Grid::GeneratorMode::Slack) {
					gen->adjustSetpointKw(correction);
				}
			}
		}
	}
}

/*
		redundant until scenario manager gets somewhere but breaker logic
*/
void Engine::processProtectionRelays() {
	std::map<Core::u64, Grid::Breaker *> breakerById;
	std::map<std::string, Grid::Breaker *> breakerByEdge;
	auto edgeKey = [](Grid::Node *a, Grid::Node *b) -> std::string {
		Core::u64 idA = a ? a->getId() : 0;
		Core::u64 idB = b ? b->getId() : 0;
		if (idA > idB)
			std::swap(idA, idB);
		return std::to_string(idA) + "-" + std::to_string(idB);
	};

	for (auto &sub : m_substations) {
		for (auto &comp : sub->getComponents()) {
			if (auto brk = dynamic_cast<Grid::Breaker *>(comp.get())) {
				breakerById[brk->getId()] = brk;
				breakerByEdge[edgeKey(brk->getFromNode(), brk->getToNode())] =
					brk;
			}
		}
	}

	for (auto &[id, recloseTick] : m_recloseAtTick) {
		if (recloseTick <= m_currentTick && breakerById.contains(id)) {
			if (m_recloseCooldownUntil.count(id) &&
				m_currentTick < m_recloseCooldownUntil[id])
				continue;
			Grid::Breaker *brk = breakerById[id];
			if (brk->isOpen()) {
				brk->setOpen(false);
				m_recloseCooldownUntil[id] = m_currentTick + 800;
				logEvent("[RELAY] Auto-reclosed breaker " + brk->getName());
			}
		}
	}
	for (auto it = m_recloseAtTick.begin(); it != m_recloseAtTick.end();) {
		if (it->second <= m_currentTick)
			it = m_recloseAtTick.erase(it);
		else
			++it;
	}

	for (auto &sub : m_substations) {
		for (auto &comp : sub->getComponents()) {
			if (auto line = dynamic_cast<Grid::Line *>(comp.get())) {
				Core::f64 iMag = std::abs(line->getCurrentFlow());
				Core::f64 lim =
					std::max<Core::f64>(1.0, line->getCurrentLimit());
				Core::f64 loadingPct = 100.0 * iMag / lim;
				m_maxObservedLineLoadingPercent =
					std::max(m_maxObservedLineLoadingPercent, loadingPct);

				std::string key =
					edgeKey(line->getFromNode(), line->getToNode());
				if (!breakerByEdge.contains(key))
					continue;
				Grid::Breaker *brk = breakerByEdge[key];

				Core::f64 pickup = 1.05 * lim;
				if (line->isDistanceRelayEnabled() && line->getFromNode() &&
					line->getToNode()) {
					Core::f64 vf = std::abs(line->getFromNode()->getVoltage()) *
								   line->getFromNode()->getBaseVoltage();
					Core::f64 vt = std::abs(line->getToNode()->getVoltage()) *
								   line->getToNode()->getBaseVoltage();
					Core::f64 zapp = std::abs(vf - vt) / std::max(1e-6, iMag);
					if (zapp < line->getDistanceReachZohm() &&
						iMag > 0.05 * lim) {
						m_pendingTrips[brk->getId()] = m_currentTick + 40;
					}
				}

				if (iMag > pickup) {
					if (!m_overloadStartTick.contains(brk->getId())) {
						m_overloadStartTick[brk->getId()] = m_currentTick;
						std::ostringstream os;
						os << "[RELAY] " << line->getName() << " overload ("
						   << std::fixed << std::setprecision(1) << loadingPct
						   << "%), trip timer started";
						logEvent(os.str());
					}
					Core::f64 ratio = iMag / lim;
					Core::f64 inv = 400.0 / std::max(ratio / 1.05 - 1.0, 0.08);
					m_pendingTrips[brk->getId()] =
						m_overloadStartTick[brk->getId()] +
						static_cast<Core::u64>(inv);
				} else {
					m_pendingTrips.erase(brk->getId());
					m_overloadStartTick.erase(brk->getId());
				}
			}
		}
	}

	for (auto it = m_pendingTrips.begin(); it != m_pendingTrips.end();) {
		Core::u64 breakerId = it->first;
		Core::u64 tripAt = it->second;
		if (tripAt <= m_currentTick && breakerById.contains(breakerId)) {
			Grid::Breaker *brk = breakerById[breakerId];
			if (!brk->isOpen()) {
				brk->setOpen(true);
				m_recloseAtTick[breakerId] = m_currentTick + 1500;
				logEvent("[RELAY] Tripped " + brk->getName() +
						 ", auto-reclose armed");
			}
			it = m_pendingTrips.erase(it);
		} else {
			++it;
		}
	}

	for (auto &stage : m_uflsStages) {
		bool rocoOk =
			stage.rocoThreshold > 1e8 || m_rocofHzPerS <= -stage.rocoThreshold;
		if (!stage.triggered && m_systemFrequency <= stage.freqThresholdHz &&
			rocoOk) {
			if (!stage.armPending) {
				stage.armPending = true;
				stage.armAtTick = m_currentTick + stage.delayTicks;
			}
			if (stage.armPending && m_currentTick >= stage.armAtTick) {
				stage.triggered = true;
				stage.armPending = false;

				std::vector<Grid::Load *> activLoads;
				for (auto &sub : m_substations)
					for (auto &comp : sub->getComponents())
						if (auto load = dynamic_cast<Grid::Load *>(comp.get()))
							if (!load->isShed())
								activLoads.push_back(load);

				size_t nShed = static_cast<size_t>(
					static_cast<Core::f64>(activLoads.size()) *
					stage.shedFraction);
				std::sort(activLoads.begin(), activLoads.end(),
						  [](auto *a, auto *b) {
							  return a->getCurrentConsumption() >
									 b->getCurrentConsumption();
						  });

				std::ostringstream os;
				os << "[UFLS] " << std::fixed << std::setprecision(2)
				   << m_systemFrequency << " Hz <= " << stage.freqThresholdHz
				   << " Hz. Shedding " << nShed << " loads.";
				logEvent(os.str());

				for (size_t i = 0; i < nShed && i < activLoads.size(); ++i)
					activLoads[i]->shed();
			}
		} else if (m_systemFrequency > stage.freqThresholdHz + 0.02) {
			stage.armPending = false;
		}

		if (stage.triggered &&
			m_systemFrequency > stage.freqThresholdHz + 0.6) {
			stage.triggered = false;
			size_t restored = 0;
			for (auto &sub : m_substations) {
				for (auto &comp : sub->getComponents()) {
					if (auto load = dynamic_cast<Grid::Load *>(comp.get())) {
						if (load->isShed() && restored < 2) {
							load->restore();
							++restored;
						}
					}
				}
			}
			if (restored > 0)
				logEvent("[UFLS] Frequency recovered, restoring staged load.");
		}
	}
}

bool Engine::openBreakerById(Core::u64 id) {
	for (auto &sub : m_substations)
		for (auto &comp : sub->getComponents())
			if (auto brk = dynamic_cast<Grid::Breaker *>(comp.get()))
				if (comp->getId() == id) {
					brk->setOpen(true);
					return true;
				}
	return false;
}

bool Engine::closeBreakerById(Core::u64 id) {
	for (auto &sub : m_substations)
		for (auto &comp : sub->getComponents())
			if (auto brk = dynamic_cast<Grid::Breaker *>(comp.get()))
				if (comp->getId() == id) {
					brk->setOpen(false);
					return true;
				}
	return false;
}

bool Engine::setLoadPowerById(Core::u64 id, Core::f64 powerKw) {
	for (auto &sub : m_substations)
		for (auto &comp : sub->getComponents())
			if (auto load = dynamic_cast<Grid::Load *>(comp.get()))
				if (comp->getId() == id) {
					load->setMaxPower(powerKw);
					return true;
				}
	return false;
}

bool Engine::setGenTargetPById(Core::u64 id, Core::f64 powerKw) {
	for (auto &sub : m_substations)
		for (auto &comp : sub->getComponents())
			if (auto gen = dynamic_cast<Grid::Generator *>(comp.get()))
				if (comp->getId() == id) {
					gen->setTargetP(powerKw);
					return true;
				}
	return false;
}

void Engine::exportVoltagesToCSV(const std::string &filename) {
	bool needsHeader =
		std::ifstream(filename).peek() == std::ifstream::traits_type::eof();
	std::ofstream file(filename, std::ios::app);
	if (!file.is_open())
		return;

	if (needsHeader) {
		Util::CSVHandler::writeRow(
			file, {"Tick", "Substation", "Node", "Voltage_Mag", "Voltage_Ang",
				   "Frequency_Hz", "ActiveShed_kW", "MaxLineLoadPct",
				   "ReserveMargin_kW"});
	}

	for (const auto &sub : m_substations) {
		for (const auto &comp : sub->getComponents()) {
			if (auto node = dynamic_cast<Grid::Node *>(comp.get())) {
				std::complex<Core::f64> voltage = node->getVoltage();
				Util::CSVHandler::writeRow(
					file, {std::to_string(m_currentTick), sub->getName(),
						   node->getName(), std::to_string(std::abs(voltage)),
						   std::to_string(std::arg(voltage)),
						   std::to_string(m_systemFrequency),
						   std::to_string(m_activeShedLoadKw),
						   std::to_string(m_maxObservedLineLoadingPercent),
						   std::to_string(m_reserveMarginKw)});
			}
		}
	}
}

void Engine::configureDemoProfiles() {
	for (auto &sub : m_substations) {
		for (auto &comp : sub->getComponents()) {
			if (auto load = dynamic_cast<Grid::Load *>(comp.get())) {
				const std::string n = load->getName();
				if (n.find("Residential") != std::string::npos)
					load->setProfile(Grid::LoadProfile::Residential);
				else if (n.find("Commercial") != std::string::npos)
					load->setProfile(Grid::LoadProfile::Commercial);
				else if (n.find("Industrial") != std::string::npos)
					load->setProfile(Grid::LoadProfile::Industrial);
				else
					load->setProfile(Grid::LoadProfile::Flat);
			}
			if (auto gen = dynamic_cast<Grid::Generator *>(comp.get())) {
				const std::string n = gen->getName();
				if (n.find("Wind") != std::string::npos)
					gen->setProfile(Grid::GeneratorProfile::Wind);
				else if (n.find("Solar") != std::string::npos)
					gen->setProfile(Grid::GeneratorProfile::Solar);
				else if (n.find("Hydro") != std::string::npos)
					gen->setProfile(Grid::GeneratorProfile::Hydro);
				else
					gen->setProfile(Grid::GeneratorProfile::Thermal);
			}
		}
	}
}

void Engine::logEvent(const std::string &event) {
	std::ostringstream os;
	os << "T+" << m_currentTick << "ms " << event;
	m_eventLog.push_back(os.str());
	if (m_eventLog.size() > 24)
		m_eventLog.pop_front();
	appendEventToCsv(m_currentTick, event);
}

void Engine::appendEventToCsv(Core::Tick tick, const std::string &event) {
	static const std::string kEventCsv = "event_log.csv";
	bool needsHeader = false;
	{
		std::ifstream probe(kEventCsv);
		needsHeader =
			!probe.good() || probe.peek() == std::ifstream::traits_type::eof();
	}

	std::ofstream file(kEventCsv, std::ios::app);
	if (!file.is_open())
		return;

	if (needsHeader)
		Util::CSVHandler::writeRow(file, {"Tick", "Event"});
	Util::CSVHandler::writeRow(file, {std::to_string(tick), event});
}

std::string Engine::getLastEvent() const {
	if (m_eventLog.empty())
		return "none";
	return m_eventLog.back();
}

void Engine::updateKpis() {
	m_frequencyNadir = std::min(m_frequencyNadir, m_systemFrequency);
	m_activeShedLoadKw = 0.0;
	m_reserveMarginKw = 0.0;

	for (auto &sub : m_substations) {
		for (auto &comp : sub->getComponents()) {
			if (auto load = dynamic_cast<Grid::Load *>(comp.get())) {
				if (load->isShed())
					m_activeShedLoadKw += load->getMaxPower();
			}
			if (auto gen = dynamic_cast<Grid::Generator *>(comp.get())) {
				if (gen->getMode() != Grid::GeneratorMode::Slack)
					m_reserveMarginKw += std::max<Core::f64>(
						0.0, gen->getMaxPower() - gen->getActualP());
			}
		}
	}
}

std::vector<std::string> Engine::getDemoScenarioNames() const {
	return {"breaker_trip", "gen_loss", "freq_stress"};
}

bool Engine::runDemoScenario(const std::string &name) {
	std::string k = name;
	for (auto &c : k)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	if (k == "breaker_trip") {
		getScenarioManager().addEvent(m_currentTick + 500,
									  [this]() { openBreakerById(22); });
		return true;
	}
	if (k == "gen_loss") {
		getScenarioManager().addEvent(
			m_currentTick + 400, [this]() { setGenTargetPById(11, 5000.0); });
		return true;
	}
	if (k == "freq_stress") {
		getScenarioManager().addEvent(
			m_currentTick + 200, [this]() { setLoadPowerById(24, 95000.0); });
		return true;
	}
	return false;
}

void Engine::clearScheduledEvents() { m_scenarioManager.clear(); }

void Engine::clearEventLog() { m_eventLog.clear(); }

bool Engine::runDeterministicDemoValidation(std::string &report) {
	initialise();
	clearScheduledEvents();
	clearEventLog();
	for (int i = 0; i < 6000; ++i)
		tick();

	bool okFreq = (m_frequencyNadir >= 47.0 && m_systemFrequency <= 52.0);
	bool okLoad = (m_totalLoad >= 0.0);
	bool ok = okFreq && okLoad;

	std::ostringstream os;
	os << "Validation " << (ok ? "PASS" : "FAIL") << " | nadir=" << std::fixed
	   << std::setprecision(2) << m_frequencyNadir
	   << "Hz, f_now=" << m_systemFrequency
	   << "Hz, maxLine=" << m_maxObservedLineLoadingPercent
	   << "%, events=" << m_eventLog.size();
	report = os.str();
	return ok;
}

/*
		IDs are not mapped to components, this is a linear search over the temporary csv
		will likely not scale well if we want to start doing ridiculous shit
		open to any suggestions about component mapping and databasing/memory indexing
*/
} // namespace GLStation::Simulation
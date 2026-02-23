#include "grid/Breaker.hpp"
#include "grid/Generator.hpp"
#include "grid/Line.hpp"
#include "grid/Load.hpp"
#include "grid/Node.hpp"
#include "grid/Transformer.hpp"
#include "io/CSVHandler.hpp"
#include "sim/Engine.hpp"
#include "sim/PowerSolver.hpp"
#include "util/Random.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace GLStation::Simulation {

/*
		load shreading thresholds
		erik you probably understand 
*/
struct UflsStage {
	Core::f64 freqThresholdHz;
	Core::f64 shedFraction;
	bool triggered;
};

static UflsStage s_uflsStages[] = {
	{49.0, 0.10, false},
	{48.7, 0.15, false},
	{48.4, 0.20, false},
	{48.0, 0.30, false},
};

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
	: m_currentTick(0), m_systemFrequency(50.0), m_totalGeneration(0),
	  m_totalLoad(0), m_totalLosses(0) {}

void Engine::initialise() {
	Util::Random::init(12345);
	m_systemFrequency = 50.0;
	m_substations.clear();
	m_currentTick = 0;

	for (auto &s : s_uflsStages)
		s.triggered = false;

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
					c = std::toupper(c);

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
					if (ctx.nodes.count(from) && ctx.nodes.count(to)) {
						auto lineObj = std::make_shared<Grid::Line>(
							name, ctx.nodes[from], ctx.nodes[to], r, x);
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
						c = std::tolower(c);
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
					if (ctx.nodes.count(pri) && ctx.nodes.count(sec)) {
						auto trafo = std::make_shared<Grid::Transformer>(
							name, ctx.nodes[pri], ctx.nodes[sec], r, x, tap);
						defaultSub->addComponent(trafo);
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
	}
	std::cout << "Grid initialised." << std::endl;
}

/*
		Fallback grid, but uh, this is sorta ass and the point at which im giving up on this project because theres a bridge between the real world 
		and demos we cant really get over at the moment
*/
void Engine::createDemoGrid() {
	auto sub = std::make_shared<Grid::Substation>("Demo");

	auto bus1 = std::make_shared<Grid::Node>("Bus1", 110.0);
	auto bus2 = std::make_shared<Grid::Node>("Bus2", 110.0);
	auto bus3 = std::make_shared<Grid::Node>("Bus3", 110.0);
	auto bus4 = std::make_shared<Grid::Node>("Bus4", 110.0);

	auto slack = std::make_shared<Grid::Generator>("SlackGen", bus1.get(),
												   Grid::GeneratorMode::Slack);
	slack->setTargetV(1.05);

	auto pv = std::make_shared<Grid::Generator>("RegenFarm", bus2.get(),
												Grid::GeneratorMode::PV);
	pv->setTargetP(30000.0);
	pv->setTargetV(1.02);

	auto solar = std::make_shared<Grid::Generator>("SolarFarm", bus4.get(),
												   Grid::GeneratorMode::PV);
	solar->setTargetP(25000.0);
	solar->setTargetV(1.02);

	auto hydro = std::make_shared<Grid::Generator>("HydroPlant", bus3.get(),
												   Grid::GeneratorMode::PV);
	hydro->setTargetP(20000.0);
	hydro->setTargetV(1.01);

	auto line1 =
		std::make_shared<Grid::Line>("L1", bus1.get(), bus2.get(), 2.0, 8.0);
	auto line2 =
		std::make_shared<Grid::Line>("L2", bus2.get(), bus3.get(), 2.0, 8.0);
	auto line3 =
		std::make_shared<Grid::Line>("L3", bus3.get(), bus4.get(), 2.0, 8.0);

	auto load2 =
		std::make_shared<Grid::Load>("CityCentre", bus2.get(), 20000.0);
	auto load3 =
		std::make_shared<Grid::Load>("IndustrialZone", bus3.get(), 35000.0);
	auto load4_residential =
		std::make_shared<Grid::Load>("ResidentialZone", bus4.get(), 18000.0);
	auto load3_commercial =
		std::make_shared<Grid::Load>("CommercialDistrict", bus3.get(), 15000.0);

	sub->addComponent(bus1);
	sub->addComponent(bus2);
	sub->addComponent(bus3);
	sub->addComponent(bus4);
	sub->addComponent(slack);
	sub->addComponent(pv);
	sub->addComponent(solar);
	sub->addComponent(hydro);
	sub->addComponent(line1);
	sub->addComponent(line2);
	sub->addComponent(line3);
	sub->addComponent(load2);
	sub->addComponent(load3);
	sub->addComponent(load4_residential);
	sub->addComponent(load3_commercial);

	m_substations.push_back(sub);
}

/*
		tick manager, need to add debug lines per component
		most of the logic here is self explanatory because of the component names
		slack and loading balacing and AGC trigger at the end of every tick, probably not the most optimised solution
*/
void Engine::tick() {
	m_currentTick++;
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
				gen->applyDroopResponse(m_systemFrequency);
			}
		}
	}

	PowerSolver::solve(m_substations, SolverSettings());

	m_totalGeneration = 0.0;
	m_totalLosses = 0.0;
	for (auto &sub : m_substations) {
		for (auto &comp : sub->getComponents()) {
			if (auto gen = dynamic_cast<Grid::Generator *>(comp.get())) {
				if (gen->getMode() != Grid::GeneratorMode::Slack)
					m_totalGeneration += gen->getActualP();
			}
			if (auto line = dynamic_cast<Grid::Line *>(comp.get()))
				m_totalLosses += line->getLosses();
			if (auto trafo = dynamic_cast<Grid::Transformer *>(comp.get()))
				m_totalLosses += trafo->getLosses();
		}
	}
	m_totalGeneration += m_totalLoad + m_totalLosses;

	updateFrequencyDynamics();
	applyAGC();
	processProtectionRelays();
}

/*
		hardcoded to 50hz still
		df/dt ∝ (P_gen - P_load - P_loss) / (2HSbase), range is 49-51(?) 
*/
void Engine::updateFrequencyDynamics() {
	const Core::f64 H = 5.0;
	const Core::f64 Sbase = 100000.0;
	const Core::f64 dt = 0.001;

	Core::f64 imbalance = m_totalGeneration - m_totalLoad - m_totalLosses;
	Core::f64 df_dt = (imbalance / (2.0 * H * Sbase)) * 50.0;
	m_systemFrequency += df_dt * dt;
	m_systemFrequency = std::clamp(m_systemFrequency, 47.0, 52.0);
}

/*
		-df * gain for slope across gens
		i might be misunderstanding the underlying logic 
		https://www.academia.edu/32675657/Automatic_Gain_Control_AGC_in_Receivers
*/
void Engine::applyAGC() {
	Core::f64 df = m_systemFrequency - 50.0;
	if (std::abs(df) < 0.005)
		return;
	Core::f64 correction = -df * 500.0;
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
	for (auto &sub : m_substations) {
		std::vector<Grid::Breaker *> breakers;
		for (auto &comp : sub->getComponents()) {
			if (auto brk = dynamic_cast<Grid::Breaker *>(comp.get()))
				breakers.push_back(brk);
		}

		for (auto &comp : sub->getComponents()) {
			if (auto line = dynamic_cast<Grid::Line *>(comp.get())) {
				Core::f64 iMag = std::abs(line->getCurrentFlow());
				if (iMag > line->getCurrentLimit()) {
					for (auto brk : breakers) {
						if (!brk->isOpen() &&
							(brk->getFromNode() == line->getFromNode() ||
							 brk->getToNode() == line->getToNode())) {
							brk->setOpen(true);
							std::cout << "\n[RELAY] OVERCURRENT on "
									  << line->getName() << " (" << std::fixed
									  << std::setprecision(1) << iMag << " A > "
									  << line->getCurrentLimit()
									  << "A limit). Tripping breaker: "
									  << brk->getName() << "\n";
							break;
						}
					}
				}
			}
		}
	}

	for (auto &stage : s_uflsStages) {
		if (!stage.triggered && m_systemFrequency <= stage.freqThresholdHz) {
			stage.triggered = true;

			std::vector<Grid::Load *> activLoads;
			for (auto &sub : m_substations)
				for (auto &comp : sub->getComponents())
					if (auto load = dynamic_cast<Grid::Load *>(comp.get()))
						if (!load->isShed())
							activLoads.push_back(load);

			size_t nShed =
				static_cast<size_t>(activLoads.size() * stage.shedFraction);
			std::sort(activLoads.begin(), activLoads.end(),
					  [](auto *a, auto *b) {
						  return a->getCurrentConsumption() >
								 b->getCurrentConsumption();
					  });

			std::cout << "\n[UFLS] Frequency " << std::fixed
					  << std::setprecision(2) << m_systemFrequency
					  << " Hz <= " << stage.freqThresholdHz << " Hz. Shedding "
					  << nShed << " loads.\n";

			for (size_t i = 0; i < nShed && i < activLoads.size(); ++i)
				activLoads[i]->shed();
		}

		if (stage.triggered &&
			m_systemFrequency > stage.freqThresholdHz + 0.3) {
			stage.triggered = false;
			for (auto &sub : m_substations)
				for (auto &comp : sub->getComponents())
					if (auto load = dynamic_cast<Grid::Load *>(comp.get()))
						load->restore();
			std::cout << "\n[UFLS] Frequency recovered to " << std::fixed
					  << std::setprecision(2) << m_systemFrequency
					  << " Hz. Restoring loads at stage "
					  << stage.freqThresholdHz << " Hz.\n";
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
			file, {"Tick", "Substation", "Node", "Voltage_Mag", "Voltage_Ang"});
	}

	for (const auto &sub : m_substations) {
		for (const auto &comp : sub->getComponents()) {
			if (auto node = dynamic_cast<Grid::Node *>(comp.get())) {
				std::complex<Core::f64> voltage = node->getVoltage();
				Util::CSVHandler::writeRow(
					file, {std::to_string(m_currentTick), sub->getName(),
						   node->getName(), std::to_string(std::abs(voltage)),
						   std::to_string(std::arg(voltage))});
			}
		}
	}
}

/*
		IDs are not mapped to components, this is a linear search over the temporary csv
		will likely not scale well if we want to start doing ridiculous shit
		open to any suggestions about component mapping and databasing/memory indexing
*/
} // namespace GLStation::Simulation

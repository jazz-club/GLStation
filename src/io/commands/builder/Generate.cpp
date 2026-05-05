#include "grid/Breaker.hpp"
#include "grid/Generator.hpp"
#include "grid/Line.hpp"
#include "grid/Load.hpp"
#include "grid/Node.hpp"
#include "grid/Shunt.hpp"
#include "grid/Substation.hpp"
#include "grid/Transformer.hpp"
#include "grid/builder/Builder.hpp"
#include "io/commands/BuilderCommands.hpp"
#include "io/handlers/InputHandler.hpp"
#include "log/Logger.hpp"
#include "sim/Engine.hpp"
#include "ui/Theme.hpp"
#include "util/Random.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace GLStation::IO::Commands::Builder {

namespace {

struct Point {
	double x, y;
};

double getDistance(Point a, Point b) {
	return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

class ProceduralGridBuilder {
  public:
	ProceduralGridBuilder(Simulation::Engine &simEngine,
						  const std::string &gridName, double targetPop)
		: engine(simEngine), name(gridName), population(targetPop) {
		baseDemandKw = population * 1.5;
		numZones = std::max(2, static_cast<int>(population / 250000.0));
	}

	void build() {
		engine.clearSubstations();

		buildNationalTie();
		buildZones();
		buildBackboneRing();
		buildCrossTies();

		std::cout << UI::Theme::cyan() << "Grid generated with " << numZones
				  << " zones and " << totalDistributedKw << " demand."
				  << UI::Theme::reset() << "\n";
	}

	std::shared_ptr<Grid::Substation> getMainSubstation() const {
		return mainSub;
	}

  private:
	Simulation::Engine &engine;
	std::string name;
	double population;
	double baseDemandKw;
	int numZones;
	double totalDistributedKw{0.0};

	std::shared_ptr<Grid::Substation> mainSub;
	std::vector<std::shared_ptr<Grid::Node>> backboneNodes;
	std::vector<Point> backboneCoords;

	void buildNationalTie() {
		mainSub = std::make_shared<Grid::Substation>(name + "_Region_Tie");
		engine.addSubstation(mainSub);

		auto slackNode = std::make_shared<Grid::Node>("Tie_400kV", 400.0);
		mainSub->addComponent(slackNode);

		auto slackGen = std::make_shared<Grid::Generator>(
			"National_Tie_Gen", slackNode.get(), Grid::GeneratorMode::Slack);
		slackGen->setTargetV(1.0);
		slackGen->setPowerBounds(
			0.0, baseDemandKw * (0.3 + Util::Random::nextDouble() * 0.6));
		mainSub->addComponent(slackGen);

		backboneNodes.push_back(slackNode);
		backboneCoords.push_back({50.0, 50.0});
	}

	void buildZones() {
		Point center{50.0, 50.0};
		for (int i = 0; i < numZones; ++i) {
			auto zoneSub = std::make_shared<Grid::Substation>(
				name + "_Zone_" + std::to_string(i));
			engine.addSubstation(zoneSub);

			Point zPoint{center.x + Util::Random::range(-40.0, 40.0),
						 center.y + Util::Random::range(-40.0, 40.0)};
			backboneCoords.push_back(zPoint);

			auto bbNode = std::make_shared<Grid::Node>(
				"Zone_" + std::to_string(i) + "_400kV", 400.0);
			auto prmNode = std::make_shared<Grid::Node>(
				"Zone_" + std::to_string(i) + "_275kV", 275.0);
			auto distNode = std::make_shared<Grid::Node>(
				"Zone_" + std::to_string(i) + "_132kV", 132.0);
			auto locNode = std::make_shared<Grid::Node>(
				"Zone_" + std::to_string(i) + "_33kV", 33.0);

			zoneSub->addComponent(bbNode);
			zoneSub->addComponent(prmNode);
			zoneSub->addComponent(distNode);
			zoneSub->addComponent(locNode);

			backboneNodes.push_back(bbNode);

			zoneSub->addComponent(std::make_shared<Grid::Transformer>(
				"Tx_400_275_" + std::to_string(i), bbNode.get(), prmNode.get(),
				0.005, 0.05, 1.0));
			zoneSub->addComponent(std::make_shared<Grid::Transformer>(
				"Tx_275_132_" + std::to_string(i), prmNode.get(),
				distNode.get(), 0.008, 0.08, 1.0));
			zoneSub->addComponent(std::make_shared<Grid::Transformer>(
				"Tx_132_33_" + std::to_string(i), distNode.get(), locNode.get(),
				0.01, 0.1, 1.0));
			zoneSub->addComponent(std::make_shared<Grid::Transformer>(
				"Tx_132_33_Redundant_" + std::to_string(i), distNode.get(),
				locNode.get(), 0.01, 0.1, 1.0));

			double zoneLoad = (baseDemandKw / numZones) *
							  (0.8 + Util::Random::nextDouble() * 0.4);
			totalDistributedKw += zoneLoad;

			addZoneLoads(zoneSub, distNode.get(), locNode.get(), zoneLoad, i);
			addZoneGeneration(zoneSub, bbNode.get(), distNode.get(),
							  locNode.get(), zoneLoad, i);
		}
	}

	void addZoneLoads(std::shared_ptr<Grid::Substation> &sub,
					  Grid::Node *distNode, Grid::Node *locNode, double loadKw,
					  int idx) {
		auto indLoad = std::make_shared<Grid::Load>(
			"Load_Ind_" + std::to_string(idx), distNode, loadKw * 0.3);
		indLoad->setProfile(Grid::LoadProfile::Industrial);
		sub->addComponent(indLoad);

		auto resLoad = std::make_shared<Grid::Load>(
			"Load_ResCom_" + std::to_string(idx), locNode, loadKw * 0.7);
		resLoad->setProfile(Util::Random::nextDouble() < 0.6
								? Grid::LoadProfile::Residential
								: Grid::LoadProfile::Commercial);
		sub->addComponent(resLoad);
	}

	void addZoneGeneration(std::shared_ptr<Grid::Substation> &sub,
						   Grid::Node *bbNode, Grid::Node *distNode,
						   Grid::Node *locNode, double loadKw, int idx) {
		if (Util::Random::nextDouble() < 0.1) {
			auto gen = std::make_shared<Grid::Generator>(
				"Baseload_Gen_" + std::to_string(idx), bbNode,
				Grid::GeneratorMode::PV);
			double cap = baseDemandKw * 0.2;
			gen->setTargetP(cap * 0.9);
			gen->setPowerBounds(0.0, cap);
			gen->setTargetV(1.02);
			gen->setQLimits(-cap * 0.5, cap * 0.5);
			sub->addComponent(gen);
		}

		if (Util::Random::nextDouble() < 0.4) {
			auto gen = std::make_shared<Grid::Generator>(
				"Generator_PV_" + std::to_string(idx), distNode,
				Grid::GeneratorMode::PV);
			double cap = loadKw * 0.5;
			gen->setTargetP(cap * 0.1);
			gen->setPowerBounds(0.0, cap);
			gen->setTargetV(1.0);
			gen->setQLimits(-cap * 0.3, cap * 0.3);
			sub->addComponent(gen);
		}

		if (Util::Random::nextDouble() < 0.8) {
			auto gen = std::make_shared<Grid::Generator>(
				"Generator_PV_" + std::to_string(idx), locNode,
				Grid::GeneratorMode::PQ);
			double cap = loadKw * 0.4;
			gen->setTargetP(cap * 0.8);
			gen->setPowerBounds(0.0, cap);
			gen->setTargetV(1.0);
			gen->setQLimits(-cap * 0.1, cap * 0.1);
			sub->addComponent(gen);
		}
	}

	void add400kVLine(Grid::Node *n1, Point p1, Grid::Node *n2, Point p2,
					  const std::string &lineName) {
		double d = std::max(1.0, getDistance(p1, p2));
		double zBase = (400.0 * 400.0) / 100.0;
		double rPu = (d * 0.03) / zBase;
		double xPu = (d * 0.3) / zBase;
		double bPu = (d * 4e-6) * zBase;

		auto line =
			std::make_shared<Grid::Line>(lineName, n1, n2, rPu, xPu, bPu);
		line->setCurrentLimit(5000.0);
		mainSub->addComponent(line);
	}

	void buildBackboneRing() {
		for (size_t i = 0; i < backboneNodes.size(); ++i) {
			size_t next = (i + 1) % backboneNodes.size();
			add400kVLine(backboneNodes[i].get(), backboneCoords[i],
						 backboneNodes[next].get(), backboneCoords[next],
						 "Ring_Line_" + std::to_string(i));
			add400kVLine(backboneNodes[i].get(), backboneCoords[i],
						 backboneNodes[next].get(), backboneCoords[next],
						 "Ring_Line_" + std::to_string(i) + "_C2");
		}
	}

	void buildCrossTies() {
		if (numZones <= 3)
			return;

		for (int i = 0; i < numZones / 2; ++i) {
			int a = 1 + static_cast<int>(Util::Random::nextU64() %
										 static_cast<Core::u64>(numZones - 1));
			int b = 1 + static_cast<int>(Util::Random::nextU64() %
										 static_cast<Core::u64>(numZones - 1));
			if (a != b) {
				add400kVLine(backboneNodes[a].get(), backboneCoords[a],
							 backboneNodes[b].get(), backboneCoords[b],
							 "CrossTie_" + std::to_string(i));
			}
		}
	}
};

} // namespace

void cmdGenerate(Simulation::Engine &engine,
				 const std::vector<std::string> &args) {
	if (args.empty() || args[0] != "grid" || args.size() < 3) {
		Log::Logger::warn("Usage: generate grid <name> <population>");
		return;
	}

	try {
		double pop = IO::InputHandler::parseDouble(args[2]);
		if (pop <= 0 || pop > 100000000.0) {
			Log::Logger::error("Population must be between 1 and 100,000,000.");
			return;
		}

		std::cout << UI::Theme::cyan() << "Generating procedural grid '"
				  << args[1] << "' for " << pop << "..." << UI::Theme::reset()
				  << "\n";

		ProceduralGridBuilder builder(engine, args[1], pop);
		builder.build();
		Grid::Builder::BuilderShell::setActiveSubstation(
			builder.getMainSubstation());
	} catch (...) {
		Log::Logger::error("Invalid format.");
	}
}

} // namespace GLStation::IO::Commands::Builder

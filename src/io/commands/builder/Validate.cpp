#include "grid/Breaker.hpp"
#include "grid/Generator.hpp"
#include "grid/Line.hpp"
#include "grid/Load.hpp"
#include "grid/Node.hpp"
#include "grid/Shunt.hpp"
#include "grid/Substation.hpp"
#include "grid/Transformer.hpp"
#include "grid/builder/Builder.hpp"
#include "io/commands/builder/Validate.hpp"
#include "sim/Engine.hpp"
#include "ui/Terminal.hpp"
#include <iostream>
#include <map>
#include <vector>

namespace GLStation::IO::Commands::Builder {

namespace {

class GridValidator {
  public:
	explicit GridValidator(Simulation::Engine &simEngine) : engine(simEngine) {}

	void run() {
		std::string red = UI::isAnsiEnabled() ? UI::ANSI_RED : "";
		std::string yel = UI::isAnsiEnabled() ? UI::ANSI_YELLOW : "";
		std::string grn = UI::isAnsiEnabled() ? UI::ANSI_GREEN : "";
		std::string cyn = UI::isAnsiEnabled() ? UI::ANSI_CYAN : "";
		std::string res = UI::isAnsiEnabled() ? UI::ANSI_RESET : "";

		std::cout << "\n" << cyn << "Grid Validation:" << res << "\n";

		scanComponents();
		printComponentCounts();

		if (allNodes.empty()) {
			std::cout << yel << "[WARN] Grid is empty.\n" << res;
			return;
		}

		checkSlackBus(red, res);
		checkGenerationCapacity(red, grn, res);
		checkTopology(red, grn, res);
	}

  private:
	Simulation::Engine &engine;
	int slackCount{0};
	int genCount{0};
	int loadCount{0};
	int lineCount{0};
	int txCount{0};
	double totalGen{0.0};
	double totalLoad{0.0};
	std::vector<Grid::Node *> allNodes;
	std::map<Grid::Node *, std::vector<Grid::Node *>> adjacency;

	void scanComponents() {
		for (auto &sub : engine.getSubstations()) {
			for (auto &comp : sub->getComponents()) {
				if (auto n = dynamic_cast<Grid::Node *>(comp.get())) {
					allNodes.push_back(n);
				} else if (auto l = dynamic_cast<Grid::Line *>(comp.get())) {
					adjacency[l->getFromNode()].push_back(l->getToNode());
					adjacency[l->getToNode()].push_back(l->getFromNode());
					lineCount++;
				} else if (auto t =
							   dynamic_cast<Grid::Transformer *>(comp.get())) {
					adjacency[t->getPrimaryNode()].push_back(
						t->getSecondaryNode());
					adjacency[t->getSecondaryNode()].push_back(
						t->getPrimaryNode());
					txCount++;
				} else if (auto b = dynamic_cast<Grid::Breaker *>(comp.get())) {
					adjacency[b->getFromNode()].push_back(b->getToNode());
					adjacency[b->getToNode()].push_back(b->getFromNode());
				} else if (auto g =
							   dynamic_cast<Grid::Generator *>(comp.get())) {
					if (g->getMode() == Grid::GeneratorMode::Slack)
						slackCount++;
					totalGen += g->getMaxPower();
					genCount++;
				} else if (auto ld = dynamic_cast<Grid::Load *>(comp.get())) {
					totalLoad += ld->getMaxPower();
					loadCount++;
				}
			}
		}
	}

	void printComponentCounts() {
		std::cout << "  Nodes: " << allNodes.size() << " | Lines: " << lineCount
				  << " | Transformers: " << txCount
				  << " | Generators: " << genCount << " | Loads: " << loadCount
				  << "\n\n";
	}

	void checkSlackBus(const std::string &red, const std::string &res) {
		if (slackCount == 0) {
			std::cout << red << "[FAIL] No Slack bus found." << res << "\n";
		} else if (slackCount > 1) {
			std::cout << red << "[FAIL] " << slackCount
					  << "Slack bus duplication error." << res << "\n";
		}
	}

	void checkGenerationCapacity(const std::string &red, const std::string &grn,
								 const std::string &res) {
		if (totalGen < totalLoad) {
			std::cout << red << "[FAIL] Total Generation Capacity (" << totalGen
					  << " kW) is less than Total Load (" << totalLoad
					  << " kW)." << res << "\n";
		} else {
			double margin =
				totalLoad > 0 ? ((totalGen / totalLoad) - 1.0) * 100.0 : 0.0;
			std::cout << grn
					  << "[PASS] Generation capacity balanced. [Sprain] Margin "
						 "for error: "
					  << margin << "%" << res << "\n";
		}
	}

	void checkTopology(const std::string &red, const std::string &grn,
					   const std::string &res) {
		std::map<Grid::Node *, bool> visited;
		std::vector<Grid::Node *> q;

		q.push_back(allNodes[0]);
		visited[allNodes[0]] = true;

		size_t head = 0;
		while (head < q.size()) {
			Grid::Node *current = q[head++];
			for (auto n : adjacency[current]) {
				if (!visited[n]) {
					visited[n] = true;
					q.push_back(n);
				}
			}
		}

		if (q.size() < allNodes.size()) {
			std::cout << red << "[FAIL] Grid contains islanded nodes"
					  << (allNodes.size() - q.size())
					  << " nodes are unreachable." << res << "\n\n";
		} else {
			std::cout << grn
					  << "[PASS] Topology is fully connected. No disconnected "
						 "nodes detected."
					  << res << "\n\n";
		}
	}
};

} // namespace

void Validate::execute(Simulation::Engine &engine, std::stringstream &ss) {
	GridValidator validator(engine);
	validator.run();
}

} // namespace GLStation::IO::Commands::Builder

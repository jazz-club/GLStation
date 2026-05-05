#include "grid/Breaker.hpp"
#include "grid/Generator.hpp"
#include "grid/Line.hpp"
#include "grid/Load.hpp"
#include "grid/Node.hpp"
#include "grid/Shunt.hpp"
#include "grid/Substation.hpp"
#include "grid/Transformer.hpp"
#include "io/commands/BuilderCommands.hpp"
#include "log/Logger.hpp"
#include "sim/Engine.hpp"
#include "ui/Theme.hpp"
#include <iostream>
#include <map>
#include <vector>

namespace GLStation::IO::Commands::Builder {

namespace {

class GridValidator {
  public:
	explicit GridValidator(Simulation::Engine &simEngine) : engine(simEngine) {}

	void run() {
		std::cout << "\n"
				  << UI::Theme::cyan()
				  << "Grid Validation:" << UI::Theme::reset() << "\n";

		scanComponents();
		printComponentCounts();

		if (allNodes.empty()) {
			Log::Logger::warn("Grid is empty.");
			return;
		}

		checkSlackBus();
		checkGenerationCapacity();
		checkTopology();
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

	void checkSlackBus() {
		if (slackCount == 0) {
			std::cout << UI::Theme::red() << "[FAIL] No Slack bus found."
					  << UI::Theme::reset() << "\n";
		} else if (slackCount > 1) {
			std::cout << UI::Theme::red() << "[FAIL] " << slackCount
					  << "Slack bus duplication error." << UI::Theme::reset()
					  << "\n";
		}
	}

	void checkGenerationCapacity() {
		if (totalGen < totalLoad) {
			std::cout << UI::Theme::red()
					  << "[FAIL] Total Generation Capacity (" << totalGen
					  << " kW) is less than Total Load (" << totalLoad
					  << " kW)." << UI::Theme::reset() << "\n";
		} else {
			double margin =
				totalLoad > 0 ? ((totalGen / totalLoad) - 1.0) * 100.0 : 0.0;
			std::cout << UI::Theme::green()
					  << "[PASS] Generation capacity balanced. [Sprain] Margin "
						 "for error: "
					  << margin << "%" << UI::Theme::reset() << "\n";
		}
	}

	void checkTopology() {
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
			std::cout << UI::Theme::red()
					  << "[FAIL] Grid contains islanded nodes"
					  << (allNodes.size() - q.size())
					  << " nodes are unreachable." << UI::Theme::reset()
					  << "\n\n";
		} else {
			std::cout << UI::Theme::green()
					  << "[PASS] Topology is fully connected. No disconnected "
						 "nodes detected."
					  << UI::Theme::reset() << "\n\n";
		}
	}
};

} // namespace

void cmdValidate(Simulation::Engine &engine,
				 const std::vector<std::string> &args) {
	(void)args;
	GridValidator validator(engine);
	validator.run();
}

} // namespace GLStation::IO::Commands::Builder

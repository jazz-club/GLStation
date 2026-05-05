#include "grid/Breaker.hpp"
#include "grid/Generator.hpp"
#include "grid/Line.hpp"
#include "grid/Load.hpp"
#include "grid/Node.hpp"
#include "grid/Transformer.hpp"
#include "io/commands/Commands.hpp"
#include "sim/Engine.hpp"
#include "ui/Theme.hpp"
#include <iomanip>
#include <iostream>
#include <map>
#include <vector>

namespace GLStation::IO::Commands {

void cmdTree(Simulation::Engine &engine, const std::vector<std::string> &args) {
	(void)args;
	std::cout << "\n"
			  << UI::Theme::cyan() << "--- Grid Topology ---"
			  << UI::Theme::reset() << std::endl;
	for (const auto &sub : engine.getSubstations()) {
		std::cout << "\n"
				  << UI::Theme::yellow() << "[Substation:" << UI::Theme::reset()
				  << " " << sub->getName() << UI::Theme::yellow() << "]"
				  << UI::Theme::reset() << std::endl;

		std::map<Grid::Node *, std::vector<Grid::GridComponent *>> nodeMap;
		for (const auto &comp : sub->getComponents()) {
			if (auto node = dynamic_cast<Grid::Node *>(comp.get())) {
				if (nodeMap.find(node) == nodeMap.end())
					nodeMap[node] = {};
			}
		}

		for (const auto &comp : sub->getComponents()) {
			if (auto gen = dynamic_cast<Grid::Generator *>(comp.get()))
				nodeMap[gen->getConnectedNode()].push_back(comp.get());
			else if (auto load = dynamic_cast<Grid::Load *>(comp.get()))
				nodeMap[load->getConnectedNode()].push_back(comp.get());
			else if (auto line = dynamic_cast<Grid::Line *>(comp.get())) {
				nodeMap[line->getFromNode()].push_back(comp.get());
			} else if (auto trafo =
						   dynamic_cast<Grid::Transformer *>(comp.get())) {
				nodeMap[trafo->getPrimaryNode()].push_back(comp.get());
			} else if (auto breaker =
						   dynamic_cast<Grid::Breaker *>(comp.get())) {
				nodeMap[breaker->getFromNode()].push_back(comp.get());
			}
		}

		for (auto const &[node, components] : nodeMap) {
			std::cout << UI::Theme::cyan() << "  |-- " << UI::Theme::reset()
					  << node->getName() << " [Node " << node->getId() << "]"
					  << std::endl;
			for (auto *comp : components) {
				std::cout << UI::Theme::cyan() << "  |   +-- "
						  << UI::Theme::reset() << "[ID: " << std::setw(2)
						  << comp->getId() << "] " << comp->getName();
				if (auto b = dynamic_cast<Grid::Breaker *>(comp))
					std::cout << (b->isOpen() ? " (OPEN)" : " (CLOSED)");
				std::cout << std::endl;
			}
		}
	}
	std::cout << std::endl;
}

} // namespace GLStation::IO::Commands

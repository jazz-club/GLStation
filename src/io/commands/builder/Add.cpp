#include "grid/Breaker.hpp"
#include "grid/Generator.hpp"
#include "grid/Line.hpp"
#include "grid/Load.hpp"
#include "grid/Node.hpp"
#include "grid/Shunt.hpp"
#include "grid/Substation.hpp"
#include "grid/Transformer.hpp"
#include "grid/builder/Builder.hpp"
#include "io/commands/builder/Add.hpp"
#include "sim/Engine.hpp"
#include <algorithm>
#include <iostream>

namespace GLStation::IO::Commands::Builder {

static double parseDouble(std::string s) {
	s.erase(std::remove(s.begin(), s.end(), ','), s.end());
	return std::stod(s);
}

static Grid::Node *findNode(Simulation::Engine &engine,
							const std::string &name) {
	for (auto &sub : engine.getSubstations()) {
		for (auto &comp : sub->getComponents()) {
			if (auto node = std::dynamic_pointer_cast<Grid::Node>(comp)) {
				if (node->getName() == name)
					return node.get();
			}
		}
	}
	return nullptr;
}

void Add::execute(Simulation::Engine &engine, std::stringstream &ss) {
	std::string type;
	ss >> type;

	auto activeSubstation = Grid::Builder::Builder::getActiveSubstation();

	if (type == "substation") {
		std::string name;
		ss >> name;
		auto sub = std::make_shared<Grid::Substation>(name);
		engine.addSubstation(sub);
		Grid::Builder::Builder::setActiveSubstation(sub);
		std::cout << "Added substation '" << name << "\n";
	} else if (type == "node") {
		std::string name, bvStr;
		if (ss >> name >> bvStr) {
			if (activeSubstation) {
				try {
					activeSubstation->addComponent(
						std::make_shared<Grid::Node>(name, parseDouble(bvStr)));
					std::cout << "Added node '" << name << "'.\n";
				} catch (...) {
					std::cout << "Invalid voltage format.\n";
				}
			} else
				std::cout << "No active substation.\n";
		} else
			std::cout << "Usage: add node <name> <baseVoltage>\n";
	} else if (type == "line") {
		std::string name, from, to, rStr, xStr, bStr;
		if (ss >> name >> from >> to >> rStr >> xStr) {
			double b = 0.0;
			if (ss >> bStr) {
				try {
					b = parseDouble(bStr);
				} catch (...) {
				}
			}
			auto n1 = findNode(engine, from);
			auto n2 = findNode(engine, to);
			if (n1 && n2 && activeSubstation) {
				try {
					activeSubstation->addComponent(std::make_shared<Grid::Line>(
						name, n1, n2, parseDouble(rStr), parseDouble(xStr), b));
					std::cout << "Added line '" << name << "'.\n";
				} catch (...) {
					std::cout << "Invalid number format.\n";
				}
			} else
				std::cout << "Nodes not found or no active substation.\n";
		} else
			std::cout << "Usage: add line <name> <from> <to> <r> <x> [b]\n";
	} else if (type == "load") {
		std::string name, node, kwStr;
		if (ss >> name >> node >> kwStr) {
			auto n = findNode(engine, node);
			if (n && activeSubstation) {
				try {
					activeSubstation->addComponent(std::make_shared<Grid::Load>(
						name, n, parseDouble(kwStr)));
					std::cout << "Added load '" << name << "'.\n";
				} catch (...) {
					std::cout << "Invalid kW format.\n";
				}
			} else
				std::cout << "Node not found or no active substation.\n";
		} else
			std::cout << "Usage: add load <name> <node> <max_kw>\n";
	} else if (type == "generator") {
		std::string name, node, modeStr, pStr, vStr;
		if (ss >> name >> node >> modeStr >> pStr >> vStr) {
			auto n = findNode(engine, node);
			if (n && activeSubstation) {
				try {
					Grid::GeneratorMode mode = Grid::GeneratorMode::PQ;
					if (modeStr == "slack")
						mode = Grid::GeneratorMode::Slack;
					else if (modeStr == "pv")
						mode = Grid::GeneratorMode::PV;
					auto gen = std::make_shared<Grid::Generator>(name, n, mode);
					gen->setTargetP(parseDouble(pStr));
					gen->setTargetV(parseDouble(vStr));
					activeSubstation->addComponent(gen);
					std::cout << "Added generator '" << name << "'.\n";
				} catch (...) {
					std::cout << "Invalid number format.\n";
				}
			} else
				std::cout << "Node not found or no active substation.\n";
		} else
			std::cout << "Usage: add generator <name> <node> "
						 "<mode(slack/pv/pq)> <target_p> <target_v>\n";
	} else if (type == "transformer") {
		std::string name, pri, sec, rStr, xStr, tapStr;
		if (ss >> name >> pri >> sec >> rStr >> xStr >> tapStr) {
			auto n1 = findNode(engine, pri);
			auto n2 = findNode(engine, sec);
			if (n1 && n2 && activeSubstation) {
				try {
					activeSubstation->addComponent(
						std::make_shared<Grid::Transformer>(
							name, n1, n2, parseDouble(rStr), parseDouble(xStr),
							parseDouble(tapStr)));
					std::cout << "Added transformer '" << name << "'.\n";
				} catch (...) {
					std::cout << "Invalid number format.\n";
				}
			} else
				std::cout << "Nodes not found or no active substation.\n";
		} else
			std::cout
				<< "Usage: add transformer <name> <pri> <sec> <r> <x> <tap>\n";
	} else if (type == "breaker") {
		std::string name, from, to;
		if (ss >> name >> from >> to) {
			auto n1 = findNode(engine, from);
			auto n2 = findNode(engine, to);
			if (n1 && n2 && activeSubstation) {
				activeSubstation->addComponent(
					std::make_shared<Grid::Breaker>(name, n1, n2));
				std::cout << "Added breaker '" << name << "'.\n";
			} else
				std::cout << "Nodes not found or no active substation.\n";
		} else
			std::cout << "Usage: add breaker <name> <from> <to>\n";
	} else if (type == "shunt") {
		std::string name, node, gStr, bStr;
		if (ss >> name >> node >> gStr >> bStr) {
			auto n = findNode(engine, node);
			if (n && activeSubstation) {
				try {
					activeSubstation->addComponent(
						std::make_shared<Grid::Shunt>(
							name, n, parseDouble(gStr), parseDouble(bStr)));
					std::cout << "Added shunt '" << name << "'.\n";
				} catch (...) {
					std::cout << "Invalid number format.\n";
				}
			} else
				std::cout << "Node not found or no active substation.\n";
		} else
			std::cout << "Usage: add shunt <name> <node> <g> <b>\n";
	} else {
		std::cout << "Unknown component type: " << type << "\n";
	}
}

} // namespace GLStation::IO::Commands::Builder

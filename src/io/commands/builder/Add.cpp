#include "grid/Battery.hpp"
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
#include <iostream>

namespace GLStation::IO::Commands::Builder {

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

void cmdAdd(Simulation::Engine &engine, const std::vector<std::string> &args) {
	if (args.empty()) {
		Log::Logger::warn("Usage: add "
						  "<substation|node|line|load|generator|transformer|"
						  "breaker|shunt> ...");
		return;
	}
	std::string type = args[0];
	auto activeSubstation = Grid::Builder::BuilderShell::getActiveSubstation();

	if (type == "substation") {
		if (args.size() < 2) {
			Log::Logger::warn("Usage: add substation <name>");
			return;
		}
		auto sub = std::make_shared<Grid::Substation>(args[1]);
		engine.addSubstation(sub);
		Grid::Builder::BuilderShell::setActiveSubstation(sub);
		Log::Logger::info("Added substation '" + args[1] + "'");
	} else if (type == "node") {
		if (args.size() < 3) {
			Log::Logger::warn("Usage: add node <name> <baseVoltage>");
			return;
		}
		if (!activeSubstation) {
			Log::Logger::error("No active substation.");
			return;
		}
		try {
			activeSubstation->addComponent(std::make_shared<Grid::Node>(
				args[1], IO::InputHandler::parseDouble(args[2])));
			Log::Logger::info("Added node '" + args[1] + "'");
		} catch (...) {
			Log::Logger::error("Invalid voltage format.");
		}
	} else if (type == "line") {
		if (args.size() < 6) {
			Log::Logger::warn("Usage: add line <name> <from> <to> <r> <x> [b]");
			return;
		}
		auto n1 = findNode(engine, args[2]);
		auto n2 = findNode(engine, args[3]);
		if (!n1 || !n2 || !activeSubstation) {
			Log::Logger::error("Nodes not found or no active substation.");
			return;
		}
		try {
			double b =
				args.size() >= 7 ? IO::InputHandler::parseDouble(args[6]) : 0.0;
			activeSubstation->addComponent(std::make_shared<Grid::Line>(
				args[1], n1, n2, IO::InputHandler::parseDouble(args[4]),
				IO::InputHandler::parseDouble(args[5]), b));
			Log::Logger::info("Added line '" + args[1] + "'");
		} catch (...) {
			Log::Logger::error("Invalid number format.");
		}
	} else if (type == "load") {
		if (args.size() < 4) {
			Log::Logger::warn("Usage: add load <name> <node> <max_kw>");
			return;
		}
		auto n = findNode(engine, args[2]);
		if (!n || !activeSubstation) {
			Log::Logger::error("Node not found or no active substation.");
			return;
		}
		try {
			activeSubstation->addComponent(std::make_shared<Grid::Load>(
				args[1], n, IO::InputHandler::parseDouble(args[3])));
			Log::Logger::info("Added load '" + args[1] + "'");
		} catch (...) {
			Log::Logger::error("Invalid kW format.");
		}
	} else if (type == "generator") {
		if (args.size() < 6) {
			Log::Logger::warn("Usage: add generator <name> <node> "
							  "<mode(slack/pv/pq)> <target_p> <target_v>");
			return;
		}
		auto n = findNode(engine, args[2]);
		if (!n || !activeSubstation) {
			Log::Logger::error("Node not found or no active substation.");
			return;
		}
		try {
			Grid::GeneratorMode mode = Grid::GeneratorMode::PQ;
			if (args[3] == "slack")
				mode = Grid::GeneratorMode::Slack;
			else if (args[3] == "pv")
				mode = Grid::GeneratorMode::PV;
			auto gen = std::make_shared<Grid::Generator>(args[1], n, mode);
			gen->setTargetP(IO::InputHandler::parseDouble(args[4]));
			gen->setTargetV(IO::InputHandler::parseDouble(args[5]));
			activeSubstation->addComponent(gen);
			Log::Logger::info("Added generator '" + args[1] + "'");
		} catch (...) {
			Log::Logger::error("Invalid number format.");
		}
	} else if (type == "transformer") {
		if (args.size() < 7) {
			Log::Logger::warn(
				"Usage: add transformer <name> <pri> <sec> <r> <x> <tap>");
			return;
		}
		auto n1 = findNode(engine, args[2]);
		auto n2 = findNode(engine, args[3]);
		if (!n1 || !n2 || !activeSubstation) {
			Log::Logger::error("Nodes not found or no active substation.");
			return;
		}
		try {
			activeSubstation->addComponent(std::make_shared<Grid::Transformer>(
				args[1], n1, n2, IO::InputHandler::parseDouble(args[4]),
				IO::InputHandler::parseDouble(args[5]),
				IO::InputHandler::parseDouble(args[6])));
			Log::Logger::info("Added transformer '" + args[1] + "'");
		} catch (...) {
			Log::Logger::error("Invalid number format.");
		}
	} else if (type == "breaker") {
		if (args.size() < 4) {
			Log::Logger::warn("Usage: add breaker <name> <from> <to>");
			return;
		}
		auto n1 = findNode(engine, args[1 + 1]);
		auto n2 = findNode(engine, args[1 + 2]);
		if (!n1 || !n2 || !activeSubstation) {
			Log::Logger::error("Nodes not found or no active substation.");
			return;
		}
		activeSubstation->addComponent(
			std::make_shared<Grid::Breaker>(args[1], n1, n2));
		Log::Logger::info("Added breaker '" + args[1] + "'");
	} else if (type == "shunt") {
		if (args.size() < 5) {
			Log::Logger::warn("Usage: add shunt <name> <node> <g> <b>");
			return;
		}
		auto n = findNode(engine, args[2]);
		if (!n || !activeSubstation) {
			Log::Logger::error("Node not found or no active substation.");
			return;
		}
		try {
			activeSubstation->addComponent(std::make_shared<Grid::Shunt>(
				args[1], n, IO::InputHandler::parseDouble(args[3]),
				IO::InputHandler::parseDouble(args[4])));
			Log::Logger::info("Added shunt '" + args[1] + "'");
		} catch (...) {
			Log::Logger::error("Invalid number format.");
		}
	} else if (type == "battery") {
		if (args.size() < 6) {
			Log::Logger::warn("Usage: add battery <name> <node> <capacity_kw> "
							  "<charge_rate_kw> <discharge_rate_kw>");
			return;
		}
		auto n = findNode(engine, args[2]);
		if (!n || !activeSubstation) {
			Log::Logger::error("Node not found or no active substation.");
			return;
		}
		try {
			activeSubstation->addComponent(std::make_shared<Grid::Battery>(
				args[1], n, IO::InputHandler::parseDouble(args[3]),
				IO::InputHandler::parseDouble(args[4]),
				IO::InputHandler::parseDouble(args[5])));
			Log::Logger::info("Added battery '" + args[1] + "'");
		} catch (...) {
			Log::Logger::error("Invalid format.");
		}
	} else {
		Log::Logger::error("Unknown component type: " + type);
	}
}

} // namespace GLStation::IO::Commands::Builder

#include "grid/Breaker.hpp"
#include "grid/Generator.hpp"
#include "grid/Line.hpp"
#include "grid/Load.hpp"
#include "grid/Node.hpp"
#include "grid/Transformer.hpp"
#include "io/handlers/CSVHandler.hpp"
#include "log/Diagnostics.hpp"
#include "log/Errors.hpp"
#include "sim/Engine.hpp"
#include <algorithm>
#include <cmath>
#include <complex>
#include <fstream>
#include <sstream>
#include <string>

namespace GLStation::Simulation {

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
	std::string target = filename.empty() ? "gls.csv" : filename;
	bool needsHeader =
		std::ifstream(target).peek() == std::ifstream::traits_type::eof();
	std::ofstream file(target, std::ios::app);
	if (!file.is_open())
		return;

	if (needsHeader) {
		Util::CSVHandler::writeRow(file,
								   {"Tick", "Category", "Message", "Node",
									"V_Mag", "V_Ang", "Freq_Hz", "Reserve_kW"});
	}

	for (const auto &sub : m_substations) {
		for (const auto &comp : sub->getComponents()) {
			if (auto node = dynamic_cast<Grid::Node *>(comp.get())) {
				std::complex<Core::f64> voltage = node->getVoltage();
				Util::CSVHandler::writeRow(
					file,
					{std::to_string(m_currentTick), "RESULT", "Voltage Export",
					 node->getName(), std::to_string(std::abs(voltage)),
					 std::to_string(std::arg(voltage)),
					 std::to_string(m_systemFrequency),
					 std::to_string(m_reserveMarginKw)});
			}
		}
	}
}
void Engine::logEvent(const std::string &event, Log::Severity sev) {
	Log::Diagnostics::record(m_currentTick, sev, event);
}

std::string Engine::getLastEvent() const {
	return Log::Diagnostics::getLastMessage();
}
/*
	reset every tick not partial rebuild
*/
void Engine::updateKpis() {
	m_frequencyNadir = m_systemFrequency;
	m_frequencyNadirLifetime =
		std::min(m_frequencyNadirLifetime, m_systemFrequency);
	m_maxLineLoadingLifetime =
		std::max(m_maxLineLoadingLifetime, m_maxObservedLineLoadingPercent);
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

/*
		IDs are not mapped to components, this is a linear search over the temporary csv
		will likely not scale well if we want to start doing ridiculous shit
		open to any suggestions about component mapping and databasing/memory indexing
*/

} // namespace GLStation::Simulation

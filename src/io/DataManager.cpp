#include "grid/Node.hpp"
#include "io/DataManager.hpp"
#include <fstream>
#include <iostream>

namespace GLStation::Simulation {

void DataManager::exportToCsv(
    const std::string &filename, Core::u64 tick,
    const std::vector<std::shared_ptr<Grid::Substation>> &substations) {
  bool needsHeader =
      std::ifstream(filename).peek() == std::ifstream::traits_type::eof();
  std::ofstream file(filename, std::ios::app);
  if (!file.is_open())
    return;

  if (needsHeader) {
    file << "Tick,Substation,Node,Voltage_Mag,Voltage_Ang\n";
  }

  for (const auto &sub : substations) {
    for (const auto &comp : sub->getComponents()) {
      if (auto node = dynamic_cast<Grid::Node *>(comp.get())) {
        std::complex<Core::f64> v = node->getVoltage();
        file << tick << "," << sub->getName() << "," << node->getName() << ","
             << std::abs(v) << "," << std::arg(v) << "\n";
      }
    }
  }
}

} // namespace GLStation::Simulation

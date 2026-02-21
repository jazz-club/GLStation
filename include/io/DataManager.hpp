#pragma once

#include "grid/Substation.hpp"
#include <memory>
#include <string>
#include <vector>

namespace GLStation::Simulation {

class DataManager {
  public:
	static void exportToCsv(
		const std::string &filename, Core::u64 tick,
		const std::vector<std::shared_ptr<Grid::Substation>> &substations);
};

} // namespace GLStation::Simulation

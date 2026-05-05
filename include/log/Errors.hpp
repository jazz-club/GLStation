#pragma once
#include "log/Diagnostics.hpp"
#include "util/Types.hpp"
#include <string>
#include <vector>

namespace GLStation::Log {

const std::vector<std::string> kEventCsvHeader = {
	"Tick",	 "Category", "Message", "Node",
	"V_Mag", "V_Ang",	 "Freq_Hz", "Reserve_kW"};

class Errors {
  public:
	static void save(Core::Tick tick, Severity sev, const std::string &msg);
};

} // namespace GLStation::Log

#pragma once
#include "log/Diagnostics.hpp"
#include "util/Types.hpp"
#include <string>

namespace GLStation::Log {

class Errors {
  public:
	static void save(Core::Tick tick, Severity sev, const std::string &msg);
};

} // namespace GLStation::Log

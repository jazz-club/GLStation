#pragma once
#include "util/Types.hpp"
#include <deque>
#include <string>
#include <vector>

namespace GLStation::Log {

enum class Severity { Info, Warning, Critical, Result };

struct DiagnosticEvent {
	Core::Tick tick;
	Severity severity;
	std::string message;
};

class Diagnostics {
  public:
	static void record(Core::Tick tick, Severity sev, const std::string &msg);
	static std::string getLastMessage();
	static std::deque<DiagnosticEvent> getHistory(size_t limit = 25);
	static void clear();

  private:
	static std::deque<DiagnosticEvent> s_eventLog;
};

} // namespace GLStation::Log

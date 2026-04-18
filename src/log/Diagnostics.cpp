#include "log/Diagnostics.hpp"
#include "log/Errors.hpp"
#include <sstream>

namespace GLStation::Log {

std::deque<DiagnosticEvent> Diagnostics::s_eventLog;

void Diagnostics::record(Core::Tick tick, Severity sev,
						 const std::string &msg) {
	DiagnosticEvent ev = {tick, sev, msg};
	s_eventLog.push_back(ev);

	if (s_eventLog.size() > 50) {
		s_eventLog.pop_front();
	}

	Errors::save(tick, sev, msg);
}

std::string Diagnostics::getLastMessage() {
	if (s_eventLog.empty())
		return "none";

	const auto &last = s_eventLog.back();
	std::ostringstream os;
	os << "T+" << last.tick << "ms " << last.message;
	return os.str();
}

std::deque<DiagnosticEvent> Diagnostics::getHistory(size_t limit) {
	if (limit >= s_eventLog.size())
		return s_eventLog;

	std::deque<DiagnosticEvent> result;
	auto it = s_eventLog.end() - limit;
	while (it != s_eventLog.end()) {
		result.push_back(*it++);
	}
	return result;
}

void Diagnostics::clear() { s_eventLog.clear(); }

} // namespace GLStation::Log

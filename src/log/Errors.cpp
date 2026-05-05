#include "io/handlers/CSVHandler.hpp"
#include "log/Errors.hpp"
#include <filesystem>
#include <fstream>

namespace GLStation::Log {

void Errors::save(Core::Tick tick, Severity sev, const std::string &msg) {
	static const std::string kMasterCsv = "gls.csv";

	bool needsHeader = !std::filesystem::exists(kMasterCsv) ||
					   std::filesystem::file_size(kMasterCsv) == 0;

	std::ofstream file(kMasterCsv, std::ios::app);
	if (!file.is_open())
		return;

	if (needsHeader) {
		IO::CSVHandler::writeRow(file, kEventCsvHeader);
	}

	std::string catStr = "INFO";
	switch (sev) {
	case Severity::Critical:
		catStr = "CRITICAL";
		break;
	case Severity::Result:
		catStr = "RESULT";
		break;
	case Severity::Warning:
		catStr = "WARNING";
		break;
	default:
		break;
	}

	if (sev != Severity::Result) {
		IO::CSVHandler::writeRow(
			file, {std::to_string(tick), catStr, msg, "", "", "", "", ""});
	}
}

} // namespace GLStation::Log

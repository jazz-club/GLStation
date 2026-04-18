#pragma once

#include <ostream>
#include <string>
#include <vector>

namespace GLStation::Util {

class CSVHandler {
  public:
	static std::string trimField(std::string s);
	static std::vector<std::string> parseCSVLine(const std::string &line);
	static void writeRow(std::ostream &out,
						 const std::vector<std::string> &fields);
};

} // namespace GLStation::Util

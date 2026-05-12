#pragma once

#include <string>

namespace GLStation::IO {

class InputHandler {
  public:
	static std::string trim(const std::string &s);
	static std::string normaliseForComparison(const std::string &s);
	static std::string urlEncode(const std::string &s);
	static std::string readLineWithHistory();
	static double parseDouble(const std::string &s);
	static bool confirmAction(const std::string &prompt);
};

} // namespace GLStation::IO

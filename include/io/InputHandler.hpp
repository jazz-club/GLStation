#pragma once

#include <string>

namespace GLStation::Util {

class InputHandler {
  public:
	static std::string trim(const std::string &s);
	static std::string normaliseForComparison(const std::string &s);
	static std::string urlEncode(const std::string &s);
};

} // namespace GLStation::Util

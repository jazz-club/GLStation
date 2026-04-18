#pragma once
#include <stdexcept>
#include <string>

namespace GLStation::Log {

class GLStationException : public std::runtime_error {
  public:
	explicit GLStationException(const std::string &msg)
		: std::runtime_error(msg) {}
};

class SimulationDivergenceException : public GLStationException {
  public:
	explicit SimulationDivergenceException(const std::string &msg)
		: GLStationException(msg) {}
};

class GridTopologyException : public GLStationException {
  public:
	explicit GridTopologyException(const std::string &msg)
		: GLStationException(msg) {}
};

} // namespace GLStation::Log

#pragma once

#include "log/Result.hpp"
#include "util/Types.hpp"
#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace GLStation::Simulation {

class GridHandler {
  public:
	static GLStation::Log::Status importCity(const std::string &cityName);
	static std::vector<std::string> getSuggestions(const std::string &query);

  private:
	struct OSMNode {
		long long id;
		double lat, lon;
		std::map<std::string, std::string> tags;
	};

	struct OSMWay {
		long long id;
		std::vector<long long> nodes;
		std::map<std::string, std::string> tags;
	};

	static std::string fetchOsmData(const std::string &cityName);
	static double calculateDistance(double lat1, double lon1, double lat2,
									double lon2);
	static void generateGridFile(const std::string &cityName,
								 const std::vector<OSMNode> &substations,
								 const std::vector<OSMWay> &lines,
								 const std::map<long long, OSMNode> &allNodes);
	static std::string sanitiseName(const std::string &s);
	static std::string osmWayName(const OSMWay &way,
								  const std::string &fallbackPrefix);
};

} // namespace GLStation::Simulation

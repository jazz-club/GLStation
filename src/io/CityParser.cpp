#include "io/CityParser.hpp"
#include "io/JsonMinimal.hpp"
#include "io/NetworkClient.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

namespace GLStation::Simulation {

static std::string urlEncode(const std::string &value) {
	std::string encoded;
	encoded.reserve(value.size() * 3);
	for (unsigned char c : value) {
		if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
			encoded += static_cast<char>(c);
		else if (c == ' ')
			encoded += "%20";
		else {
			char buf[4];
			std::snprintf(buf, sizeof(buf), "%%%02X", c);
			encoded += buf;
		}
	}
	return encoded;
}

/*
		OSM is baby please dont feed baby unsanitised name
*/
std::string CityParser::sanitiseName(const std::string &s) {
	std::string out;
	out.reserve(s.size());
	for (unsigned char c : s) {
		if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == ' ')
			out += static_cast<char>(c);
	}
	std::replace(out.begin(), out.end(), ' ', '_');
	std::string collapsed;
	bool prevUnderscore = false;
	for (char c : out) {
		if (c == '_') {
			if (!prevUnderscore)
				collapsed += c;
			prevUnderscore = true;
		} else {
			collapsed += c;
			prevUnderscore = false;
		}
	}
	while (!collapsed.empty() && collapsed.back() == '_')
		collapsed.pop_back();
	while (!collapsed.empty() && collapsed.front() == '_')
		collapsed.erase(0, 1);
	return collapsed.empty() ? "unnamed" : collapsed;
}

std::string CityParser::osmWayName(const OSMWay &way,
								   const std::string &fallbackPrefix) {
	if (way.tags.count("name") && !way.tags.at("name").empty())
		return sanitiseName(way.tags.at("name"));
	if (way.tags.count("ref") && !way.tags.at("ref").empty())
		return sanitiseName(way.tags.at("ref"));
	if (way.tags.count("operator") && !way.tags.at("operator").empty())
		return sanitiseName(way.tags.at("operator") + "_" +
							std::to_string(way.id));
	return fallbackPrefix + "_" + std::to_string(way.id);
}

bool CityParser::importCity(const std::string &cityName) {
	std::cout << "Importing grid for: " << cityName << "..." << std::endl;

	std::string json = fetchOsmData(cityName);
	if (json.empty()) {
		std::cerr << "Error: OSM API." << std::endl;
		return false;
	}

	Util::JsonValue root = Util::JsonMinimal::parse(json);
	const Util::JsonValue &elements = root["elements"];

	std::vector<OSMNode> substations;
	std::vector<OSMWay> lines;
	std::map<long long, OSMNode> allNodes;

	for (size_t i = 0; i < elements.size(); ++i) {
		const auto &el = elements[i];
		std::string type = el["type"].str;

		if (type == "node") {
			OSMNode node;
			node.id = (long long)el["id"].number;
			node.lat = el["lat"].number;
			node.lon = el["lon"].number;

			if (el.contains("tags")) {
				for (auto const &[key, val] : el["tags"].object) {
					node.tags[key] = val.str;
				}
			}

			allNodes[node.id] = node;
			if (node.tags.count("power") &&
				node.tags["power"] == "substation") {
				substations.push_back(node);
			}
		} else if (type == "way") {
			OSMWay way;
			way.id = (long long)el["id"].number;
			const auto &nodesArr = el["nodes"];
			for (size_t j = 0; j < nodesArr.size(); ++j) {
				way.nodes.push_back((long long)nodesArr[j].number);
			}
			if (el.contains("tags")) {
				for (auto const &[key, val] : el["tags"].object) {
					way.tags[key] = val.str;
				}
			}
			if (way.tags.count("power") && way.tags["power"] == "line") {
				lines.push_back(way);
			}
		}
	}

	if (substations.empty()) {
		std::cerr
			<< "No substations found for that area. The name may not match "
			   "an OSM dataset, there is missing powergrid data."
			<< std::endl;
		return false;
	}

	std::cout << "Found " << substations.size() << " substations and "
			  << lines.size() << " transmission lines." << std::endl;
	generateGridFile(cityName, substations, lines, allNodes);
	return true;
}

std::string CityParser::fetchOsmData(const std::string &cityName) {
	Util::NetworkClient client;
	std::string query =
		"[out:json];area[name=\"" + cityName +
		"\"]->.searchArea;(way[\"power\"=\"line\"](area.searchArea);node["
		"\"power\"=\"substation\"](area.searchArea););out body;>;out skel qt;";

	Util::NetworkResponse res =
		client.post("https://overpass-api.de/api/interpreter", "data=" + query);
	if (res.success)
		return res.body;
	return "";
}

/*
		i think haversince difference is the easiest way to calculate this but i might be wrong
		still unutilised but i reckoned this would factor into impedance over distance 
*/
double CityParser::calculateDistance(double lat1, double lon1, double lat2,
									 double lon2) {
	const double R = 6371.0;
	double dLat = (lat2 - lat1) * Core::PI / 180.0;
	double dLon = (lon2 - lon1) * Core::PI / 180.0;
	double a = sin(dLat / 2) * sin(dLat / 2) +
			   cos(lat1 * Core::PI / 180.0) * cos(lat2 * Core::PI / 180.0) *
				   sin(dLon / 2) * sin(dLon / 2);
	double c = 2 * atan2(sqrt(a), sqrt(1 - a));
	return R * c;
}

void CityParser::generateGridFile(
	const std::string &cityName, const std::vector<OSMNode> &substations,
	const std::vector<OSMWay> &lines,
	const std::map<long long, OSMNode> &allNodes) {
	std::ofstream file("grid.csv");
	std::string subName = sanitiseName(cityName);
	if (subName.empty() || subName == "unnamed")
		subName = "Imported_Grid";

	file << "SUBSTATION," << subName << "\n";

	std::map<long long, std::string> nodeNames;
	for (const auto &sub : substations) {
		std::string name;
		if (sub.tags.count("name") && !sub.tags.at("name").empty())
			name = sanitiseName(sub.tags.at("name"));
		else if (sub.tags.count("ref") && !sub.tags.at("ref").empty())
			name = "Sub_" + sanitiseName(sub.tags.at("ref"));
		else
			name = "Substation_" + std::to_string(sub.id);

		double kv = 110.0;
		if (sub.tags.count("voltage")) {
			try {
				std::string vStr = sub.tags.at("voltage");
				kv = std::stod(vStr.substr(0, vStr.find(';'))) / 1000.0;
			} catch (...) {
			}
		}

		file << "NODE," << name << "," << kv << "\n";
		nodeNames[sub.id] = name;
		file << "LOAD,Load_" << name << "," << name << ",10000.0\n";
	}

	if (!substations.empty()) {
		std::string firstNode = nodeNames[substations[0].id];
		std::string slackName = "Slack_" + firstNode;
		file << "GEN," << slackName << "," << firstNode
			 << ",slack,0.0,1.05,-1000000.0,1000000.0\n";
	}

	for (const auto &way : lines) {
		if (way.nodes.size() < 2)
			continue;

		long long startNodeId = -1;
		long long endNodeId = -1;
		for (long long nid : way.nodes) {
			if (nodeNames.count(nid)) {
				if (startNodeId == -1)
					startNodeId = nid;
				else {
					endNodeId = nid;
					break;
				}
			}
		}

		if (startNodeId != -1 && endNodeId != -1) {
			const auto &n1 = allNodes.at(startNodeId);
			const auto &n2 = allNodes.at(endNodeId);
			double dist = calculateDistance(n1.lat, n1.lon, n2.lat, n2.lon);
			double r = 0.01 * dist;
			double x = 0.1 * dist;
			std::string lineName = osmWayName(way, "Line");
			file << "LINE," << lineName << "," << nodeNames[startNodeId] << ","
				 << nodeNames[endNodeId] << "," << r << "," << x << "\n";
		}
	}

	file.close();
	std::cout << "Grid Updated." << std::endl;
}

std::vector<std::string> CityParser::getSuggestions(const std::string &query) {
	std::vector<std::string> out;
	if (query.empty())
		return out;

	std::string url =
		"https://nominatim.openstreetmap.org/search?q=" + urlEncode(query) +
		"&format=json&limit=5";
	Util::NetworkClient client;
	Util::NetworkResponse res = client.get(url);
	if (!res.success || res.body.empty())
		return out;

	Util::JsonValue root = Util::JsonMinimal::parse(res.body);
	if (root.type != Util::JsonValue::Type::Array)
		return out;

	for (size_t i = 0; i < root.size(); ++i) {
		const auto &el = root[i];
		if (el.contains("display_name") && !el["display_name"].str.empty())
			out.push_back(el["display_name"].str);
	}
	return out;
}

} // namespace GLStation::Simulation

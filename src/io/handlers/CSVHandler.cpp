#include "io/handlers/CSVHandler.hpp"
#include <cctype>

namespace GLStation::Util {

std::string CSVHandler::trimField(std::string s) {
	while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
		s.pop_back();
	while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
		s.erase(0, 1);
	return s;
}

std::vector<std::string> CSVHandler::parseCSVLine(const std::string &line) {
	std::vector<std::string> out;
	std::string field;
	for (size_t i = 0; i < line.size(); ++i) {
		if (line[i] == '"') {
			field.clear();
			++i;
			while (i < line.size() && line[i] != '"') {
				field += line[i++];
			}
			out.push_back(field);
		} else if (line[i] == ',') {
			out.push_back(trimField(field));
			field.clear();
		} else {
			field += line[i];
		}
	}
	out.push_back(trimField(field));
	return out;
}

void CSVHandler::writeRow(std::ostream &out,
						  const std::vector<std::string> &fields) {
	for (size_t i = 0; i < fields.size(); ++i) {
		if (i > 0)
			out << ',';
		const std::string &fieldStr = fields[i];
		bool quote = false;
		for (char ch : fieldStr) {
			if (ch == ',' || ch == '"' || ch == '\n' || ch == '\r') {
				quote = true;
				break;
			}
		}
		if (quote) {
			out << '"';
			for (char ch : fieldStr) {
				if (ch == '"')
					out << "\"\"";
				else
					out << ch;
			}
			out << '"';
		} else {
			out << fieldStr;
		}
	}
	out << '\n';
}

} // namespace GLStation::Util

#include "io/handlers/CSVHandler.hpp"
#include <cctype>

namespace GLStation::IO {

std::string CSVHandler::trimField(std::string s) {
	while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
		s.pop_back();
	while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
		s.erase(0, 1);
	return s;
}

bool CSVHandler::isCommentOrEmpty(const std::string &line) {
	for (char c : line) {
		if (std::isspace(static_cast<unsigned char>(c)))
			continue;
		return c == '#';
	}
	return true;
}

std::vector<std::string> CSVHandler::parseCSVLine(const std::string &line) {
	std::vector<std::string> out;
	std::string field;
	bool inQuotes = false;

	for (size_t i = 0; i < line.size(); ++i) {
		char c = line[i];

		if (inQuotes) {
			if (c == '"') {
				if (i + 1 < line.size() && line[i + 1] == '"') {
					field += '"';
					++i;
				} else {
					inQuotes = false;
				}
			} else {
				field += c;
			}
		} else {
			if (c == '"') {
				inQuotes = true;
			} else if (c == ',') {
				out.push_back(trimField(field));
				field.clear();
			} else {
				field += c;
			}
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

} // namespace GLStation::IO

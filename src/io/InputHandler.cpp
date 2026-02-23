#include "io/InputHandler.hpp"
#include <cctype>
#include <cstdio>

namespace GLStation::Util {

static bool isAsciiSpace(unsigned char c) {
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static size_t utf8SeqLength(unsigned char c) {
	if (c < 128)
		return 1;
	if (c >= 194 && c <= 223)
		return 2;
	if (c >= 224 && c <= 239)
		return 3;
	if (c >= 240 && c <= 244)
		return 4;
	return 1;
}

std::string InputHandler::trim(const std::string &s) {
	size_t start = 0;
	while (start < s.size() &&
		   isAsciiSpace(static_cast<unsigned char>(s[start])))
		++start;
	size_t end = s.size();
	while (end > start && isAsciiSpace(static_cast<unsigned char>(s[end - 1])))
		--end;
	return (start < end) ? s.substr(start, end - start) : std::string();
}

std::string InputHandler::normaliseForComparison(const std::string &s) {
	std::string trimmed = trim(s);
	std::string out;
	out.reserve(trimmed.size());
	for (size_t i = 0; i < trimmed.size();) {
		unsigned char c = static_cast<unsigned char>(trimmed[i]);
		if (c < 128) {
			out += static_cast<char>(std::tolower(c));
			++i;
		} else {
			size_t seqLen = utf8SeqLength(c);
			for (size_t j = 0; j < seqLen && i + j < trimmed.size(); ++j)
				out += trimmed[i + j];
			i += seqLen;
		}
	}
	return out;
}

std::string InputHandler::urlEncode(const std::string &s) {
	std::string encoded;
	encoded.reserve(s.size() * 3);
	for (unsigned char c : s) {
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

} // namespace GLStation::Util

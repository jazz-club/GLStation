#include "io/JsonHandler.hpp"
#include <cctype>

namespace GLStation::Util {

void JsonHandler::skipWhitespace(const std::string &json, size_t &pos) {
	while (pos < json.length() &&
		   isspace(static_cast<unsigned char>(json[pos])))
		pos++;
}

JsonHandler::Value JsonHandler::parseValue(const std::string &json,
										   size_t &pos) {
	skipWhitespace(json, pos);
	if (pos >= json.length())
		return {};

	char c = json[pos];
	if (c == '{')
		return parseObject(json, pos);
	if (c == '[')
		return parseArray(json, pos);
	if (c == '"')
		return parseString(json, pos);
	if (isdigit(static_cast<unsigned char>(c)) || c == '-')
		return parseNumber(json, pos);
	if (pos + 4 <= json.length() && json.substr(pos, 4) == "true") {
		pos += 4;
		Value value;
		value.type = Value::Type::Bool;
		value.boolean = true;
		return value;
	}
	if (pos + 5 <= json.length() && json.substr(pos, 5) == "false") {
		pos += 5;
		Value value;
		value.type = Value::Type::Bool;
		value.boolean = false;
		return value;
	}
	if (pos + 4 <= json.length() && json.substr(pos, 4) == "null") {
		pos += 4;
		return {};
	}

	return {};
}

JsonHandler::Value JsonHandler::parseObject(const std::string &json,
											size_t &pos) {
	Value val;
	val.type = Value::Type::Object;
	pos++;
	while (true) {
		skipWhitespace(json, pos);
		if (pos >= json.length())
			break;
		if (json[pos] == '}') {
			pos++;
			break;
		}
		Value key = parseString(json, pos);
		skipWhitespace(json, pos);
		if (pos >= json.length() || json[pos] != ':')
			break;
		pos++;
		val.object[key.str] = parseValue(json, pos);
		skipWhitespace(json, pos);
		if (pos >= json.length())
			break;
		if (json[pos] == ',')
			pos++;
		else if (json[pos] == '}') {
			pos++;
			break;
		}
	}
	return val;
}

JsonHandler::Value JsonHandler::parseArray(const std::string &json,
										   size_t &pos) {
	Value val;
	val.type = Value::Type::Array;
	pos++;
	while (true) {
		skipWhitespace(json, pos);
		if (pos >= json.length())
			break;
		if (json[pos] == ']') {
			pos++;
			break;
		}
		val.array.push_back(parseValue(json, pos));
		skipWhitespace(json, pos);
		if (pos >= json.length())
			break;
		if (json[pos] == ',')
			pos++;
		else if (json[pos] == ']') {
			pos++;
			break;
		}
	}
	return val;
}

JsonHandler::Value JsonHandler::parseString(const std::string &json,
											size_t &pos) {
	Value val;
	val.type = Value::Type::String;
	pos++;
	while (pos < json.length() && json[pos] != '"') {
		if (json[pos] == '\\')
			pos++;
		val.str += json[pos++];
	}
	if (pos < json.length())
		pos++;
	return val;
}

JsonHandler::Value JsonHandler::parseNumber(const std::string &json,
											size_t &pos) {
	Value val;
	val.type = Value::Type::Number;
	size_t start = pos;
	while (pos < json.length() &&
		   (isdigit(static_cast<unsigned char>(json[pos])) ||
			json[pos] == '.' || json[pos] == '-' || json[pos] == 'e' ||
			json[pos] == 'E' || json[pos] == '+')) {
		pos++;
	}
	try {
		val.number = std::stod(json.substr(start, pos - start));
	} catch (...) {
		val.number = 0;
	}
	return val;
}

JsonHandler::Value JsonHandler::parse(const std::string &json) {
	size_t pos = 0;
	return parseValue(json, pos);
}

} // namespace GLStation::Util

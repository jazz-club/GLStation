#pragma once

#include <iostream>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace GLStation::Util {

struct JsonValue {
	enum class Type { Object, Array, String, Number, Null, Bool };
	Type type = Type::Null;

	std::map<std::string, JsonValue> object;
	std::vector<JsonValue> array;
	std::string str;
	double number = 0;
	bool boolean = false;

	const JsonValue &operator[](const std::string &key) const {
		auto it = object.find(key);
		static const JsonValue null_val;
		return (it != object.end()) ? it->second : null_val;
	}

	const JsonValue &operator[](size_t index) const {
		if (index < array.size())
			return array[index];
		static const JsonValue null_val;
		return null_val;
	}

	bool contains(const std::string &key) const {
		return object.count(key) > 0;
	}
	size_t size() const {
		return (type == Type::Array) ? array.size() : object.size();
	}
};

class JsonMinimal {
  public:
	static JsonValue parse(const std::string &json) {
		size_t pos = 0;
		return parseValue(json, pos);
	}

  private:
	static void skipWhitespace(const std::string &json, size_t &pos) {
		while (pos < json.length() && isspace(json[pos]))
			pos++;
	}

	static JsonValue parseValue(const std::string &json, size_t &pos) {
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
		if (isdigit(c) || c == '-')
			return parseNumber(json, pos);
		if (json.substr(pos, 4) == "true") {
			pos += 4;
			JsonValue v;
			v.type = JsonValue::Type::Bool;
			v.boolean = true;
			return v;
		}
		if (json.substr(pos, 5) == "false") {
			pos += 5;
			JsonValue v;
			v.type = JsonValue::Type::Bool;
			v.boolean = false;
			return v;
		}
		if (json.substr(pos, 4) == "null") {
			pos += 4;
			return {};
		}

		return {};
	}

	static JsonValue parseObject(const std::string &json, size_t &pos) {
		JsonValue val;
		val.type = JsonValue::Type::Object;
		pos++;
		while (true) {
			skipWhitespace(json, pos);
			if (json[pos] == '}') {
				pos++;
				break;
			}
			JsonValue key = parseString(json, pos);
			skipWhitespace(json, pos);
			if (json[pos] != ':')
				break;
			pos++;
			val.object[key.str] = parseValue(json, pos);
			skipWhitespace(json, pos);
			if (json[pos] == ',')
				pos++;
			else if (json[pos] == '}') {
				pos++;
				break;
			}
		}
		return val;
	}

	static JsonValue parseArray(const std::string &json, size_t &pos) {
		JsonValue val;
		val.type = JsonValue::Type::Array;
		pos++;
		while (true) {
			skipWhitespace(json, pos);
			if (json[pos] == ']') {
				pos++;
				break;
			}
			val.array.push_back(parseValue(json, pos));
			skipWhitespace(json, pos);
			if (json[pos] == ',')
				pos++;
			else if (json[pos] == ']') {
				pos++;
				break;
			}
		}
		return val;
	}

	static JsonValue parseString(const std::string &json, size_t &pos) {
		JsonValue val;
		val.type = JsonValue::Type::String;
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

	static JsonValue parseNumber(const std::string &json, size_t &pos) {
		JsonValue val;
		val.type = JsonValue::Type::Number;
		size_t start = pos;
		while (pos < json.length() &&
			   (isdigit(json[pos]) || json[pos] == '.' || json[pos] == '-' ||
				json[pos] == 'e' || json[pos] == 'E' || json[pos] == '+')) {
			pos++;
		}
		try {
			val.number = std::stod(json.substr(start, pos - start));
		} catch (...) {
			val.number = 0;
		}
		return val;
	}
};

} // namespace GLStation::Util

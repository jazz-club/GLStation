#pragma once

#include <map>
#include <string>
#include <vector>

namespace GLStation::Util {

class JsonHandler {
  public:
	struct Value {
		enum class Type { Object, Array, String, Number, Null, Bool };
		Type type = Type::Null;

		std::map<std::string, Value> object;
		std::vector<Value> array;
		std::string str;
		double number = 0;
		bool boolean = false;

		const Value &operator[](const std::string &key) const {
			auto it = object.find(key);
			static const Value nullVal;
			return (it != object.end()) ? it->second : nullVal;
		}

		const Value &operator[](size_t index) const {
			if (index < array.size())
				return array[index];
			static const Value nullVal;
			return nullVal;
		}

		bool contains(const std::string &key) const {
			return object.count(key) > 0;
		}
		size_t size() const {
			return (type == Type::Array) ? array.size() : object.size();
		}
	};

	static Value parse(const std::string &json);

  private:
	static void skipWhitespace(const std::string &json, size_t &pos);
	static Value parseValue(const std::string &json, size_t &pos);
	static Value parseObject(const std::string &json, size_t &pos);
	static Value parseArray(const std::string &json, size_t &pos);
	static Value parseString(const std::string &json, size_t &pos);
	static Value parseNumber(const std::string &json, size_t &pos);
};

} // namespace GLStation::Util

#pragma once
#include <string>

namespace GLStation::Log {

enum class ErrorCode {
	Success,
	NotFound,
	ParseError,
	NetworkError,
	SimulationError,
	Unknown
};

template <typename T> struct Result {
	T value;
	ErrorCode code;
	std::string message;

	bool isSuccess() const { return code == ErrorCode::Success; }
};

struct Status {
	ErrorCode code;
	std::string message;

	bool isSuccess() const { return code == ErrorCode::Success; }
};

} // namespace GLStation::Log

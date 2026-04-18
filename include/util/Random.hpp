#pragma once

#include "util/Types.hpp"

namespace GLStation::Util {

class Random {
  public:
	static void init(::GLStation::Core::u64 seed);
	static ::GLStation::Core::u64 nextU64();
	static ::GLStation::Core::f64 nextDouble();
	static ::GLStation::Core::f64 range(::GLStation::Core::f64 min,
										::GLStation::Core::f64 max);

  private:
	static ::GLStation::Core::u64 s_state;
};

} // namespace GLStation::Util

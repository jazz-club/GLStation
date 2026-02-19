#include "util/Random.hpp"

namespace GLStation::Util {

Core::u64 Random::s_state = 0xCAFEBABE12345678ULL;

void Random::init(Core::u64 seed) {
  s_state = seed;
  nextU64();
  nextU64();
}

Core::u64 Random::nextU64() {
  Core::u64 z = (s_state += 0x9E3779B97F4A7C15ULL);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

Core::f64 Random::nextDouble() {
  return static_cast<Core::f64>(nextU64() >> 11) * (1.0 / (1ULL << 53));
}

Core::f64 Random::range(Core::f64 min, Core::f64 max) {
  return min + (max - min) * nextDouble();
}

} // namespace GLStation::Util

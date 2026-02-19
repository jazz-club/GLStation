#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace GLStation::Core {

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using f32 = float;
using f64 = double;

constexpr f64 PI = 3.14159265358979323846;
constexpr f64 EPSILON = std::numeric_limits<f64>::epsilon();

using Tick = u64;
constexpr Tick INVALID_TICK = std::numeric_limits<Tick>::max();

} // namespace GLStation::Core

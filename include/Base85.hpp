#pragma once
#include <cstdint>
#include <cstddef>
#include <span>

namespace Base85
{

// Returns the span of output bytes actually used
std::span<char8_t> Encode(
	const std::span<const std::uint32_t> Input, const std::span<char8_t> Output
);

void Decode(
	std::span<const char8_t> Input, std::span<char8_t> Output
);

// Filters a given array of bytes so that all `0` and `1` bytes are filtered
// towards the front of the array, and returns the new length of the array
std::size_t Filter(std::span<char8_t> Bytes);

}

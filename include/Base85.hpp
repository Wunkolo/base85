#pragma once
#include <cstdint>
#include <cstddef>
#include <span>

namespace Base85
{

void Encode(
	std::span<const char8_t> Input, std::span<char8_t> Output
);

void Decode(
	std::span<const char8_t> Input, std::span<char8_t> Output
);

// Filters a given array of bytes so that all `0` and `1` bytes are filtered
// towards the front of the array, and returns the new length of the array
std::size_t Filter(std::span<char8_t> Bytes);

}

#pragma once
#include <cstdint>
#include <cstddef>
#include <span>

namespace Base85
{

void Encode(
	std::span<const std::uint8_t> Input, std::span<std::uint8_t> Output
);

void Decode(
	std::span<const std::uint8_t> Input, std::span<std::uint8_t> Output
);

// Filters a given array of bytes so that all `0` and `1` bytes are filtered
// towards the front of the array, and returns the new length of the array
std::size_t Filter(std::span<std::uint8_t> Bytes);

}

#pragma once
#include <cstdint>
#include <cstddef>

namespace Base85
{

void Encode(
	const std::uint8_t Input[], std::uint64_t Output[], std::size_t Length
);

void Decode(
	const std::uint64_t Input[], std::uint8_t Output[], std::size_t Length
);

// Filters a given array of bytes so that all `0` and `1` bytes are filtered
// towards the front of the array, and returns the new length of the array
std::size_t Filter(std::uint8_t Bytes[], std::size_t Length);

}

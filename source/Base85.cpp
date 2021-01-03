#include <Base85.hpp>
#include <algorithm>
#include <cassert>

//#if defined(__x86_64__) || defined(_M_X64)
//#include "Base85-x86.hpp"
//#else
// Generic Implementation

constexpr std::array<std::uint32_t, 5> Pow85 = {{
	52200625ul, 614125ul, 7225ul, 85ul, 1ul
}};

void Base85::Encode(
	std::span<const char8_t> Input, std::span<char8_t> Output
)
{
	assert( ((Input.size() / 4) * 5) == Output.size() );
	for( std::size_t i = 0; i < Input.size() / 4; ++i )
	{
		const std::uint32_t InTuple = __builtin_bswap32(
			*reinterpret_cast<const std::uint32_t*>(&Input[i * 4])
		);
		const auto OutTuple = Output.subspan(i * 5, 5);
		OutTuple[0] = ((InTuple / Pow85[0]) % 85ul) + u8'!';
		OutTuple[1] = ((InTuple / Pow85[1]) % 85ul) + u8'!';
		OutTuple[2] = ((InTuple / Pow85[2]) % 85ul) + u8'!';
		OutTuple[3] = ((InTuple / Pow85[3]) % 85ul) + u8'!';
		OutTuple[4] = ((InTuple / Pow85[4]) % 85ul) + u8'!';
	}
}

void Base85::Decode(
	std::span<const char8_t> Input, std::span<char8_t> Output
)
{
	for( std::size_t i = 0; i < Input.size() / 5; ++i )
	{
		const auto InTuple = Input.subspan(i * 5, 5);
		std::uint32_t& OutTuple = *reinterpret_cast<std::uint32_t*>(&Output[i * 4]);
		OutTuple = 0;
		for( std::size_t j = 0; j < 5; ++j )
		{
			OutTuple += (InTuple[j] - u8'!') * Pow85[j];
		}
		OutTuple = __builtin_bswap32(OutTuple);
	}
}

std::size_t Base85::Filter( std::span<char8_t> Bytes )
{
	return  std::remove_if(
		Bytes.data(), Bytes.data() + Bytes.size(),
		[](const char8_t& CurByte) -> bool
		{
			return ( CurByte < u8'!' || CurByte > u8'u' );
		}
	) - Bytes.data();
}
//#endif

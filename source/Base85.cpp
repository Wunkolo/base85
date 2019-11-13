#include <Base85.hpp>
#include <algorithm>

//#if defined(__x86_64__) || defined(_M_X64)
//#include "Base85-x86.hpp"
//#else
// Generic Implementation
void Base85::Encode(
	const std::uint8_t Input[], std::size_t Length, std::uint8_t Output[]
)
{
	for( std::size_t i = 0; i < Length / 4; ++i )
	{
		std::uint32_t InTuple = __builtin_bswap32(
			*reinterpret_cast<const std::uint32_t*>(&Input[i * 4])
		);
		for( std::size_t j = 0; j < 5; ++j )
		{
			Output[i * 5 + (4u - j)] = InTuple % 85 + '!';
			InTuple /= 85;
		}
	}
}

void Base85::Decode(
	const std::uint8_t Input[], std::size_t Length, std::uint8_t Output[]
)
{
	for( std::size_t i = 0; i < Length / 5; ++i )
	{
		const std::uint32_t Pow85[5] = {
			52200625ul, 614125ul, 7225ul, 85ul, 1ul
		};
		std::uint32_t& OutTuple = *reinterpret_cast<std::uint32_t*>(&Output[i * 4]);
		OutTuple = 0;
		for( std::size_t j = 0; j < 5; ++j )
		{
			OutTuple += (Input[i * 5 + j] - '!') * Pow85[j];
		}
		OutTuple = __builtin_bswap32(OutTuple);
	}
}

std::size_t Base85::Filter(std::uint8_t Bytes[], std::size_t Length)
{
	return  std::remove_if(
		Bytes, Bytes + Length,
		[](const std::uint8_t& CurByte) -> bool
		{
			return ( CurByte < '!' || CurByte > 'u' );
		}
	) - Bytes;
}
//#endif

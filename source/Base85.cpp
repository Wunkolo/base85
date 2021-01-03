#include <Base85.hpp>
#include <algorithm>

//#if defined(__x86_64__) || defined(_M_X64)
//#include "Base85-x86.hpp"
//#else
// Generic Implementation
void Base85::Encode(
	std::span<const std::uint8_t> Input, std::uint8_t Output[]
)
{
	const std::uint32_t Pow85[5] = {
		52200625ul, 614125ul, 7225ul, 85ul, 1ul
	};
	for( std::size_t i = 0; i < Input.size() / 4; ++i )
	{
		const std::uint32_t InTuple = __builtin_bswap32(
			*reinterpret_cast<const std::uint32_t*>(&Input[i * 4])
		);
		Output[i * 5 + 0] = ((InTuple / Pow85[0]) % 85ul) + '!';
		Output[i * 5 + 1] = ((InTuple / Pow85[1]) % 85ul) + '!';
		Output[i * 5 + 2] = ((InTuple / Pow85[2]) % 85ul) + '!';
		Output[i * 5 + 3] = ((InTuple / Pow85[3]) % 85ul) + '!';
		Output[i * 5 + 4] = ((InTuple / Pow85[4]) % 85ul) + '!';
	}
}

void Base85::Decode(
	std::span<const std::uint8_t> Input, std::uint8_t Output[]
)
{
	const std::uint32_t Pow85[5] = {
		52200625ul, 614125ul, 7225ul, 85ul, 1ul
	};
	for( std::size_t i = 0; i < Input.size() / 5; ++i )
	{
		std::uint32_t& OutTuple = *reinterpret_cast<std::uint32_t*>(&Output[i * 4]);
		OutTuple = 0;
		for( std::size_t j = 0; j < 5; ++j )
		{
			OutTuple += (Input[i * 5 + j] - '!') * Pow85[j];
		}
		OutTuple = __builtin_bswap32(OutTuple);
	}
}

std::size_t Base85::Filter(std::span<std::uint8_t> Bytes)
{
	return  std::remove_if(
		Bytes.data(), Bytes.data() + Bytes.size(),
		[](const std::uint8_t& CurByte) -> bool
		{
			return ( CurByte < '!' || CurByte > 'u' );
		}
	) - Bytes.data();
}
//#endif

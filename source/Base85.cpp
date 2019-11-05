#include <Base85.hpp>

//#if defined(__x86_64__) || defined(_M_X64)
//#include "Base85-x86.hpp"
//#else
// Generic Implementation
void Base85::Encode(
	const std::uint8_t Input[], std::uint64_t Output[], std::size_t Length
)
{
	// Least significant bit in an 8-bit integer
	constexpr std::uint64_t LSB8       = 0x0101010101010101UL;
	// Each byte has a unique bit set
	constexpr std::uint64_t UniqueBit  = 0x0102040810204080UL;
	// Shifts unique bits to the left, using the carry of binary addition
	constexpr std::uint64_t CarryShift = 0x7F7E7C7870604000UL;
	// Most significant bit in an 8-bit integer
	constexpr std::uint64_t MSB8       = LSB8 << 7u;
	// Constant bits for ascii '0' and '1'
	constexpr std::uint64_t BinAsciiBasis = LSB8 * '0';
	for( std::size_t i = 0; i < Length; ++i )
	{
		Output[i] = ((((((
			static_cast<std::uint64_t>(Input[i])
			* LSB8			) // "broadcast" low byte to all 8 bytes.
			& UniqueBit		) // Mask each byte to have 1 unique bit.
			+ CarryShift	) // Shift this bit to the last bit of each
							  // byte using the carry of binary addition.
			& MSB8			) // Isolate these last bits of each byte.
			>> 7			) // Shift it back to the low bit of each byte.
			| BinAsciiBasis	  // Turn it into ascii '0' and '1'
		);
	}
}

void Base85::Decode(
	const std::uint64_t Input[], std::uint8_t Output[], std::size_t Length
)
{
	for( std::size_t i = 0; i < Length; ++i )
	{
		std::uint8_t Binary = 0;
		std::uint64_t Mask = 0x0101010101010101UL;
		for( std::uint64_t CurBit = 1UL; Mask != 0; CurBit <<= 1 )
		{
			if( __builtin_bswap64(Input[i]) & Mask & -Mask )
			{
				Binary |= CurBit;
			}
			Mask &= (Mask - 1UL);
		}
		Output[i] = Binary;
	}
}

std::size_t Base85::Filter(std::uint8_t Bytes[], std::size_t Length)
{
	std::size_t End = 0;
	for( std::size_t i = 0; i < Length; ++i )
	{
		const std::uint8_t CurByte = Bytes[i];
		if( (CurByte & 0b11111110) != 0x30 ) continue;
		Bytes[End++] = CurByte;
	}
	return End;
}
//#endif

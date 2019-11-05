#include <x86intrin.h>

/// Encoding

namespace
{

// Recursive device
template<std::uint8_t WidthExp2>
inline void Encode(
	const std::uint8_t Input[], std::uint64_t Output[], std::size_t Length
)
{
	Encode<WidthExp2-1>(Input, Output, Length);
}

// Serial
template<>
inline void Encode<0>(
	const std::uint8_t Input[], std::uint64_t Output[], std::size_t Length
)
{
	// Least significant bit in an 8-bit integer
	constexpr std::uint64_t LSB8       = 0x0101010101010101UL;
	// Constant bits for ascii '0' and '1'
	constexpr std::uint64_t BinAsciiBasis = LSB8 * '0';
#if defined (__BMI2__)
	for( std::size_t i = 0; i < Length; ++i )
	{
		Output[i] = __builtin_bswap64(
			_pdep_u64(
				static_cast<std::uint64_t>(Input[i]), LSB8
			) | BinAsciiBasis
		);
	}
#else
	for( std::size_t i = 0; i < Length; ++i )
	{
		constexpr std::uint64_t UniqueBit  = 0x0102040810204080UL;
		constexpr std::uint64_t CarryShift = 0x7F7E7C7870604000UL;
		constexpr std::uint64_t MSB8       = LSB8 << 7u;
		Output[i] = ((((((
			static_cast<std::uint64_t>(Input[i])
			* LSB8			) & UniqueBit		)
			+ CarryShift	) & MSB8			)
			>> 7			) | BinAsciiBasis	);
	}
#endif
}

// Two at a time
#if defined(__SSE2__)
template<>
inline void Encode<1>(
	const std::uint8_t Input[], std::uint64_t Output[], std::size_t Length
)
{
	constexpr std::uint64_t LSB8          = 0x0101010101010101UL;
	constexpr std::uint64_t UniqueBit     = 0x0102040810204080UL;
	constexpr std::uint64_t CarryShift    = 0x7F7E7C7870604000UL;

	std::size_t i = 0;
	for( ; i < Length; i += 2 )
	{
	#if defined(__SSSE3__)
		__m128i Result = _mm_set1_epi16(
			*reinterpret_cast<const std::uint16_t*>(&Input[i])
		);
		// Upper and lower 64-bits get filled with bytes
		Result = _mm_shuffle_epi8(Result, _mm_set_epi64x(LSB8 * 1, LSB8 * 0));
	#else
		__m128i Result = _mm_set_epi64x(
			LSB8 * static_cast<std::uint64_t>(Input[i + 1]),
			LSB8 * static_cast<std::uint64_t>(Input[i + 0])
		);
	#endif
		// Mask Unique bits per byte
		Result = _mm_and_si128(Result, _mm_set1_epi64x(UniqueBit));
		// Use the carry-bit to slide it to the far left
		Result = _mm_add_epi64(Result, _mm_set1_epi64x(CarryShift));
	#if defined(__SSE4_1__)
		// Pick between ascii '0' and '1', using the upper bit in each byte
		Result = _mm_blendv_epi8(
			_mm_set1_epi8('0'), _mm_set1_epi8('1'), Result
		);
	#else
		constexpr std::uint64_t BinAsciiBasis = LSB8 * '0';
		constexpr std::uint64_t MSB8          = LSB8 << 7u;
		// Mask this last bit
		Result = _mm_and_si128(Result, _mm_set1_epi64x(MSB8));
		// Shift it to the low bit of each byte
		Result = _mm_srli_epi64(Result, 7);
		// Convert it to ascii `0` and `1`
		Result = _mm_or_si128(Result, _mm_set1_epi64x(BinAsciiBasis));
	#endif
		_mm_store_si128(reinterpret_cast<__m128i*>(&Output[i]), Result);
	}

	Encode<0>(Input + i * 2, Output + i * 2, Length % 2);
}
#endif

#if defined(__AVX2__)
// Four at a time
template<>
inline void Encode<2>(
	const std::uint8_t Input[], std::uint64_t Output[], std::size_t Length
)
{
	constexpr std::uint64_t LSB8       = 0x0101010101010101UL;
	constexpr std::uint64_t UniqueBit  = 0x0102040810204080UL;
	constexpr std::uint64_t CarryShift = 0x7F7E7C7870604000UL;

	std::size_t i = 0;
	for( ; i < Length; i += 4 )
	{
		__m256i Result = _mm256_set1_epi32(
			*reinterpret_cast<const std::uint32_t*>(&Input[i])
		);
		// Broadcast each byte to each 64-bit lane
		Result = _mm256_shuffle_epi8(
			Result, _mm256_set_epi64x(LSB8 * 3, LSB8 * 2, LSB8 * 1, LSB8 * 0)
		);
		// Mask Unique bits per byte
		Result = _mm256_and_si256(Result, _mm256_set1_epi64x(UniqueBit));
		// Use the carry-bit of addition to slide it to the sign bit
		Result = _mm256_add_epi64(Result, _mm256_set1_epi64x(CarryShift));
		// Pick between ascii '0' and '1', using the upper bit in each byte
		Result = _mm256_blendv_epi8(
			_mm256_set1_epi8('0'), _mm256_set1_epi8('1'), Result
		);
		_mm256_storeu_si256( reinterpret_cast<__m256i*>(&Output[i]), Result);
	}

	Encode<1>(Input + i * 4, Output + i * 4, Length % 4);
}
#endif

#if defined(__AVX512F__) && defined(__AVX512BITALG__)
template<>
inline void Encode<3>(
	const std::uint8_t Input[], std::uint64_t Output[], std::size_t Length
)
{
	constexpr std::uint64_t LSB8          = 0x0101010101010101UL;

	std::size_t i = 0;
	for( ; i < Length; i += 8 )
	{
		// Reverse bits in each byte and convert it into an AVX512 mask,
		// all in one instruction.
		const __mmask64 Mask = _mm512_bitshuffle_epi64_mask(
			_mm512_set1_epi64(*(const std::uint64_t*)&Input[i]),
			_mm512_set_epi64(
				0x00'01'02'03'04'05'06'07 + LSB8 * 0x38, // Byte 7
				0x00'01'02'03'04'05'06'07 + LSB8 * 0x30, // Byte 6
				0x00'01'02'03'04'05'06'07 + LSB8 * 0x28, // Byte 5
				0x00'01'02'03'04'05'06'07 + LSB8 * 0x20, // Byte 4
				0x00'01'02'03'04'05'06'07 + LSB8 * 0x18, // Byte 3
				0x00'01'02'03'04'05'06'07 + LSB8 * 0x10, // Byte 2
				0x00'01'02'03'04'05'06'07 + LSB8 * 0x08, // Byte 1
				0x00'01'02'03'04'05'06'07 + LSB8 * 0x00  // Byte 0
			)
		);
		// Use 64-bit mask to create 64 ascii-bytes(8 encoded bytes)
		// by picking between '0' and '1' bytes
		const __m512i Ascii = _mm512_mask_blend_epi8(
			Mask, _mm512_set1_epi8('0'), _mm512_set1_epi8('1')
		);
		_mm512_storeu_si512(&Output[i], Ascii);
	}

	Encode<2>(Input + i * 8, Output + i * 8, Length % 8);
}
#elif defined(__AVX512F__) && defined(__AVX512BW__)
// Eight at a time
template<>
inline void Encode<3>(
	const std::uint8_t Input[], std::uint64_t Output[], std::size_t Length
)
{
	constexpr std::uint64_t LSB8          = 0x0101010101010101UL;
	constexpr std::uint64_t UniqueBit     = 0x0102040810204080UL;

	std::size_t i = 0;
	for( ; i < Length; i += 8 )
	{
		// Load 8 bytes, and broadcast it across all 8 64-bit lanes
		__m512i Bytes8 = _mm512_set1_epi64(
			*reinterpret_cast<const std::uint64_t*>(&Input[i])
		);
		// "Unzip" each byte across each 64-bit lane
		Bytes8 = _mm512_shuffle_epi8(
			Bytes8, _mm512_set_epi64(
				LSB8 * 7, LSB8 * 6, LSB8 * 5, LSB8 * 4,
				LSB8 * 3, LSB8 * 2, LSB8 * 1, LSB8 * 0
			)
		);
		// Get unique bits in each byte into a 64-bit mask
		const __mmask64 BitMask = _mm512_test_epi8_mask(
			Bytes8, _mm512_set1_epi64(UniqueBit)
		);
		// Use the mask to select between ASCII bytes `0` and `1`
		const __m512i ASCII = _mm512_mask_blend_epi8(
			BitMask, _mm512_set1_epi8('0'), _mm512_set1_epi8('1')
		);
		_mm512_storeu_si512(reinterpret_cast<__m512i*>(&Output[i]), ASCII);
	}

	Encode<2>(Input + i * 8, Output + i * 8, Length % 8);
}
#endif
}


void Base85::Encode(
	const std::uint8_t Input[], std::uint64_t Output[], std::size_t Length
)
{
	::Encode<0xFFu>(Input, Output, Length);
}


/// Decoding

namespace
{

// Recursive device
template<std::uint8_t WidthExp2>
inline void Decode(
	const std::uint64_t Input[], std::uint8_t Output[], std::size_t Length
)
{
	Decode<WidthExp2-1>(Input, Output, Length);
}


// Serial
template<>
inline void Decode<0>(
	const std::uint64_t Input[], std::uint8_t Output[], std::size_t Length
)
{
	for( std::size_t i = 0; i < Length; ++i )
	{
		std::uint8_t Binary = 0;
		const std::uint64_t ASCII = __builtin_bswap64(Input[i]);
	#if defined(__BMI2__)
		Binary = _pext_u64(ASCII, 0x0101010101010101UL);
	#else
		std::uint64_t Mask = 0x0101010101010101UL;
		for( std::uint64_t CurBit = 1UL; Mask != 0; CurBit <<= 1 )
		{
			if( ASCII & Mask & -Mask )
			{
				Binary |= CurBit;
			}
			Mask &= (Mask - 1UL);
		}
	#endif
		Output[i] = Binary;
	}
}

// Two at a time
#if defined(__SSE2__)
template<>
inline void Decode<1>(
	const std::uint64_t Input[], std::uint8_t Output[], std::size_t Length
)
{
	constexpr std::uint64_t LSB8 = 0x0101010101010101UL;
	std::size_t i = 0;
	for( ; i < Length; i += 2 )
	{
		// Load in 16 bytes of endian-swapped ascii bytes
	#if defined(__SSSE3__)
		__m128i ASCII = _mm_loadu_si128(
			reinterpret_cast<const __m128i*>(&Input[i])
		);
		ASCII = _mm_shuffle_epi8(
			ASCII,
			_mm_set_epi64x(
				0x0001020304050607 + LSB8 * 0x08,
				0x0001020304050607 + LSB8 * 0x00
			)
		);
	#else
		__m128i ASCII = _mm_set_epi64x(
			__builtin_bswap64(Input[i + 1]), __builtin_bswap64(Input[i + 0])
		);
	#endif
		// Shift lowest bit of each byte into sign bit
		ASCII = _mm_slli_epi64(ASCII, 7);
		// Compress each sign bit into a 16-bit word
		*reinterpret_cast<std::uint16_t*>(&Output[i]) = _mm_movemask_epi8(ASCII);
	}

	Decode<0>(Input + i * 2, Output + i * 2, Length % 2);
}
#endif

// Four at a time
#if defined(__AVX2__)
template<>
inline void Decode<2>(
	const std::uint64_t Input[], std::uint8_t Output[], std::size_t Length
)
{
	constexpr std::uint64_t LSB8 = 0x0101010101010101UL;
	std::size_t i = 0;
	for( ; i < Length; i += 4 )
	{
		// Load in 32 bytes of endian-swapped ascii bytes
		__m256i ASCII = _mm256_loadu_si256(
			reinterpret_cast<const __m256i*>(&Input[i])
		);
		// Reverse each 8-byte element in each 128-bit lane
		ASCII = _mm256_shuffle_epi8(
			ASCII,
			_mm256_set_epi64x(
				0x0001020304050607 + LSB8 * 0x08,
				0x0001020304050607 + LSB8 * 0x00,
				0x0001020304050607 + LSB8 * 0x08,
				0x0001020304050607 + LSB8 * 0x00
			)
		);
		// Shift lowest bit of each byte into sign bit
		ASCII = _mm256_slli_epi64(ASCII, 7);
		*reinterpret_cast<std::uint32_t*>(&Output[i]) = _mm256_movemask_epi8(ASCII);
	}

	Decode<1>(Input + i * 4, Output + i * 4, Length % 4);
}
#endif

// Eight at a time
#if defined(__AVX512F__) && defined(__AVX512BITALG__)
template<>
inline void Decode<3>(
	const std::uint64_t Input[], std::uint8_t Output[], std::size_t Length
)
{
	std::size_t i = 0;
	for( ; i < Length; i += 8 )
	{
		const __mmask64 Compressed = _mm512_bitshuffle_epi64_mask(
			_mm512_loadu_si512(reinterpret_cast<const __m512i*>(Input + i)),
			// Samples ascii bits in an endian-swapped order
			_mm512_set1_epi64(0x00'08'10'18'20'28'30'38)
		);
		_store_mask64(reinterpret_cast<__mmask64*>(Output + i), Compressed);
	}

	Decode<2>(Input + i * 8, Output + i * 8, Length % 8);
}
#elif defined(__AVX512F__) && defined(__AVX512BW__)
template<>
inline void Decode<3>(
	const std::uint64_t Input[], std::uint8_t Output[], std::size_t Length
)
{
	constexpr std::uint64_t LSB8 = 0x0101010101010101UL;
	std::size_t i = 0;
	for( ; i < Length; i += 8 )
	{
		// Load in 64 bytes of endian-swapped ascii bytes
		__m512i ASCII = _mm512_loadu_si512(
			reinterpret_cast<const __m512i*>(&Input[i])
		);
		ASCII = _mm512_shuffle_epi8(
			ASCII,
			_mm512_set_epi64(
				0x0001020304050607 + LSB8 * 0x38,
				0x0001020304050607 + LSB8 * 0x30,
				0x0001020304050607 + LSB8 * 0x28,
				0x0001020304050607 + LSB8 * 0x20,
				0x0001020304050607 + LSB8 * 0x18,
				0x0001020304050607 + LSB8 * 0x10,
				0x0001020304050607 + LSB8 * 0x08,
				0x0001020304050607 + LSB8 * 0x00
			)
		);
		const __mmask64 Binary = _mm512_test_epi8_mask(
			ASCII, _mm512_set1_epi8(0x01)
		);
		*reinterpret_cast<std::uint64_t*>(&Output[i]) = _cvtmask64_u64(Binary);
	}

	Decode<2>(Input + i * 8, Output + i * 8, Length % 8);
}
#endif
}

void Base85::Decode(
	const std::uint64_t Input[], std::uint8_t Output[], std::size_t Length
)
{
	::Decode<0xFFu>(Input, Output, Length);
}

/// Filtering

std::size_t Base85::Filter(std::uint8_t Bytes[], std::size_t Length)
{
	std::size_t End = 0;
	std::size_t i = 0;

	#if defined(__AVX512F__) && defined(__AVX512BW__)
	// Check and compress 16 bytes at a time
	for( ; i + 63 < Length; i += 64 )
	{
		// Read in 16 bytes at once
		const __m512i Word512 = _mm512_loadu_si512(
			reinterpret_cast<const __m512i*>(Bytes + i)
		);

		// Check for valid bytes, in parallel
		const __mmask64 BinaryTest = _mm512_cmpeq_epi8_mask(
			_mm512_and_si512(Word512, _mm512_set1_epi8(0xFE)),
			_mm512_set1_epi8(0x30)
		);
		if( _ktestc_mask64_u8(0, BinaryTest) )
		{
			// We have 16 valid ascii-binary bytes
			_mm512_storeu_si512(
				reinterpret_cast<__m512i*>(Bytes + End), Word512
			);
			End += 64;
		}
		else
		{
			// There is garbage
			for( std::size_t k = 0; k < 64; ++k )
			{
				const std::uint8_t CurByte = Bytes[i + k];
				if( (CurByte & 0xFE) != 0x30 ) continue;
				Bytes[End++] = CurByte;
			}
		}
	}
	#elif defined(__AVX2__)
	// Check and compress 16 bytes at a time
	for( ; i + 31 < Length; i += 32 )
	{
		// Read in 16 bytes at once
		const __m256i Word256 = _mm256_loadu_si256(
			reinterpret_cast<const __m256i*>(Bytes + i)
		);

		// Check for valid bytes, in parallel
		const std::uint32_t BinaryTest = _mm256_movemask_epi8(
			_mm256_cmpeq_epi8(
				_mm256_and_si256(Word256, _mm256_set1_epi8(0xFE)),
				_mm256_set1_epi8(0x30)
			)
		);
		if( BinaryTest == std::uint32_t(~0u) )
		{
			// We have 16 valid ascii-binary bytes
			_mm256_storeu_si256(
				reinterpret_cast<__m256i*>(Bytes + End), Word256
			);
			End += 32;
		}
		else
		{
			// There is garbage
			for( std::size_t k = 0; k < 32; ++k )
			{
				const std::uint8_t CurByte = Bytes[i + k];
				if( (CurByte & 0xFE) != 0x30 ) continue;
				Bytes[End++] = CurByte;
			}
		}
	}
	#elif defined(__SSE2__)
	// Check and compress 16 bytes at a time
	for( ; i + 15 < Length; i += 16 )
	{
		// Read in 16 bytes at once
		const __m128i Word128 = _mm_loadu_si128(
			reinterpret_cast<const __m128i*>(Bytes + i)
		);

		// Check for valid bytes, in parallel
		const __m128i BinaryTest = _mm_cmpeq_epi8(
			_mm_and_si128(Word128, _mm_set1_epi8(0xFE)),
			_mm_set1_epi8(0x30)
		);
		if( _mm_movemask_epi8(BinaryTest) == 0xFFFF )
		{
			// We have 16 valid ascii-binary bytes
			_mm_storeu_si128(
				reinterpret_cast<__m128i*>(Bytes + End), Word128
			);
			End += 16;
		}
		else
		{
			// There is garbage
			for( std::size_t k = 0; k < 16; ++k )
			{
				const std::uint8_t CurByte = Bytes[i + k];
				if( (CurByte & 0xFE) != 0x30 ) continue;
				Bytes[End++] = CurByte;
			}
		}
	}
	#else
	// Check and compress 8 bytes at a time
	for( ; i + 7 < Length; i += 8 )
	{
		// Read in 8 bytes at once
		const std::uint64_t Word64 = *reinterpret_cast<const std::uint64_t*>(Bytes + i);

		// Check for valid bytes, in parallel
		if( (Word64 & 0xFEFEFEFEFEFEFEFE) == 0x3030303030303030 )
		{
			// We have 8 valid ascii-binary bytes
			*reinterpret_cast<std::uint64_t*>(Bytes + End) = Word64;
			End += 8;
		}
		else
		{
			// There is garbage
			for( std::size_t k = 0; k < 8; ++k )
			{
				const std::uint8_t CurByte = Bytes[i + k];
				if( (CurByte & 0xFE) != 0x30 ) continue;
				Bytes[End++] = CurByte;
			}
		}
	}
	#endif

	for( ; i < Length; ++i )
	{
		const std::uint8_t CurByte = Bytes[i];
		if( (CurByte & 0xFE) != 0x30 ) continue;
		Bytes[End++] = CurByte;
	}
	return End;
}

#include "Base85.hpp"
#include <algorithm>

#include <catch2/catch.hpp>
#include <string>

namespace Catch
{
template<>
struct StringMaker<std::u8string>
{
	static std::string convert(const std::u8string& value)
	{
		return std::string(value.begin(), value.end());
	}
};

} // namespace Catch

// Special-case of four zero-bytes resulting in a single 'z' character
TEST_CASE("Zero-Tuple", "[Base85]")
{
	const std::u8string Input(4, '\0');

	std::u8string Output;
	Output.resize((Input.size() / 4) * 5);

	const auto OutputSpan = Base85::Encode(
		std::span(
			reinterpret_cast<const std::uint32_t*>(Input.data()),
			Input.size() / 4),
		Output);

	REQUIRE(
		std::u8string_view(OutputSpan.data(), OutputSpan.size_bytes())
		== u8"z");
}

TEST_CASE("Zero-Tuple x512", "[Base85]")
{
	const std::u8string Input(4 * 512, '\0');

	std::u8string Output;
	Output.resize((Input.size() / 4) * 5);

	const auto OutputSpan = Base85::Encode(
		std::span(
			reinterpret_cast<const std::uint32_t*>(Input.data()),
			Input.size() / 4),
		Output);

	REQUIRE(std::all_of(OutputSpan.begin(), OutputSpan.end(), [](char8_t Char) {
		return Char == u8'z';
	}));
}

TEST_CASE("Alphanumeric", "[Base85]")
{
	const std::u8string Input(u8"abcdefghijklmnopqrstuvwxyz0123456789");

	std::u8string Output;
	Output.resize((Input.size() / 4) * 5);

	Base85::Encode(
		std::span(
			reinterpret_cast<const std::uint32_t*>(Input.data()),
			Input.size() / 4),
		Output);

	REQUIRE(Output == u8"@:E_WAS,RgBkhF\"D/O92EH6,BF`qtRH$V/!1,CaE2E*TU");
}

// Unit-test from the base85 wikipedia
TEST_CASE("Thomas Hobbes's Leviathan", "[Base85]")
{
	std::u8string Input(
		u8"Man is distinguished, not only by his reason, but by this singular "
		u8"passion from other animals, which is a lust of the mind, that by a "
		u8"perseverance of delight in the continued and indefatigable "
		u8"generation of knowledge, exceeds the short vehemence of any carnal "
		u8"pleasure.");

	// Add padding bytes if not aligned to 4-bytes
	const std::size_t PaddingSize = (4u - Input.size() % 4u) % 4u;
	Input.append(PaddingSize, u8'\0');

	const std::u8string Match
		= u8"9jqo^BlbD-BleB1DJ+*+F(f,q/0JhKF<GL>Cj@.4Gp$d7F!,L7@<6@)/"
		  u8"0JDEF<G%<+EV:2F!,O<DJ+*.@<*K0@<6L(Df-\\0Ec5e;DffZ(EZee.Bl."
		  u8"9pF\"AGXBPCsi+DGm>@3BB/F*&OCAfu2/"
		  u8"AKYi(DIb:@FD,*)+C]U=@3BN#EcYf8ATD3s@q?d$AftVqCh[NqF<G:8+EV:.+Cf>-"
		  u8"FD5W8ARlolDIal(DId<j@<?3r@:F%a+D58\'ATD4$Bl@l3De:,-DJs`8ARoFb/"
		  u8"0JMK@qB4^F!,R<AKZ&-DfTqBG%G>uD.RTpAKYo\'+CT/5+Cei#DII?(E,9)oF*2M7/"
		  u8"c";

	std::u8string Output;
	Output.resize(((Input.size() / 4)) * 5);

	Base85::Encode(
		std::span(
			reinterpret_cast<const std::uint32_t*>(Input.data()),
			Input.size() / 4),
		Output);

	Output.resize(Output.size() - PaddingSize);

	REQUIRE(Output == Match);
}
#include "Base85.hpp"

#include <catch2/catch.hpp>
#include <string>

TEST_CASE("Test", "[Base85]")
{
	const std::u8string Input(4, '\0');
	std::u8string       Output;
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
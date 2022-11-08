#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <sys/mman.h>
#include <unistd.h>

#include <Base85.hpp>

// Virtual page size of the current system, rounded to nearest multiple of 4
const static std::size_t DecodedBuffSize = (sysconf(_SC_PAGE_SIZE) / 4) * 4;
// Each 4 bytes of input matches up to 5 bytes of output
const static std::size_t EncodedBuffSize = (DecodedBuffSize / 4) * 5;

struct Settings
{
	std::FILE*  InputFile     = stdin;
	std::FILE*  OutputFile    = stdout;
	bool        Decode        = false;
	bool        IgnoreInvalid = false;
	std::size_t Wrap          = 76;
};

static std::size_t WrapWrite(
	std::span<const char8_t> Buffer, std::size_t WrapWidth,
	std::FILE* OutputFile, std::size_t CurrentColumn = 0)
{
	if( WrapWidth == 0 )
	{
		// Width of 0 is just a plain output
		return std::fwrite(Buffer.data(), 1, Buffer.size(), OutputFile);
	}
	for( std::size_t Written = 0; Written < Buffer.size(); )
	{
		const std::size_t ColumnsRemaining = WrapWidth - CurrentColumn;
		const std::size_t ToWrite
			= std::min(ColumnsRemaining, Buffer.size() - Written);
		if( ToWrite == 0 )
		{
			std::fputc('\n', OutputFile);
			CurrentColumn = 0;
		}
		else
		{
			std::fwrite(Buffer.data() + Written, 1, ToWrite, OutputFile);
			CurrentColumn += ToWrite;
			Written += ToWrite;
		}
	}
	return CurrentColumn;
}

static bool Encode(const Settings& Settings)
{
	// Every 4 bytes input will map to 5 bytes of output
	std::span<std::uint32_t> InputBuffer(
		static_cast<std::uint32_t*>(mmap(
			0, DecodedBuffSize, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)),
		DecodedBuffSize / 4);
	std::span<char8_t> OutputBuffer(
		static_cast<char8_t*>(mmap(
			0, EncodedBuffSize, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)),
		EncodedBuffSize);
	std::size_t CurrentColumn = 0;
	std::size_t CurRead       = 0;
	while(
		(CurRead = std::fread(
			 InputBuffer.data(), 1, DecodedBuffSize, Settings.InputFile)) )
	{
		const std::size_t   Padding     = (4u - CurRead % 4u) % 4u;
		const std::uint32_t PaddingMask = 0xFFFFFFFFu >> Padding;
		// Add padding 0x00 bytes to last element, if needed
		if( Padding )
			InputBuffer[CurRead / 4] &= PaddingMask;
		// Round up to nearest multiple of 4
		CurRead += Padding;
		// Every four bytes matches up to up to 5 bytes, so prepare for at
		// at-least the worst case output size
		const auto InputSpan  = InputBuffer.first(CurRead / 4);
		auto       OutputSpan = Base85::Encode(InputSpan, OutputBuffer);
		// Because we added padding, we must remove it from the output.
		if( Padding )
			OutputSpan = OutputSpan.first(OutputSpan.size() - Padding);

		CurrentColumn = WrapWrite(
			// Remove padding byte values from output,
			OutputSpan, Settings.Wrap, Settings.OutputFile, CurrentColumn);
	}
	if( std::ferror(Settings.InputFile) )
	{
		std::fputs("Error while reading input file", stderr);
	}
	munmap(InputBuffer.data(), DecodedBuffSize);
	munmap(OutputBuffer.data(), EncodedBuffSize);
	return EXIT_SUCCESS;
}

static bool Decode(const Settings& Settings)
{
	// Every 5 bytes of input will map to 4 byte of output
	std::span<char8_t> InputBuffer(
		static_cast<char8_t*>(mmap(
			0, EncodedBuffSize, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)),
		EncodedBuffSize);
	std::span<char8_t> OutputBuffer(
		static_cast<char8_t*>(mmap(
			0, DecodedBuffSize, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)),
		DecodedBuffSize);

	std::size_t ToRead = EncodedBuffSize;
	// Number of bytes available for actual processing
	std::size_t CurRead = 0;
	// Process paged-sized batches of input in an attempt to have bulk-amounts
	// of conversions going on between calls to `read`
	while(
		(CurRead = std::fread(
			 reinterpret_cast<char8_t*>(InputBuffer.data())
				 + (EncodedBuffSize - ToRead),
			 1, ToRead, Settings.InputFile)) )
	{
		// Filter input of all garbage bytes
		if( Settings.IgnoreInvalid )
		{
			CurRead = Base85::Filter(
				InputBuffer.subspan(EncodedBuffSize - ToRead, CurRead));
		}
		const std::size_t Padding = (5u - CurRead % 5u) % 5u;
		// Add padding 0x00 bytes
		for( std::size_t i = 0; i < Padding; ++i )
			InputBuffer[CurRead + i] = 'u';
		// Round up to nearest multiple of 4
		CurRead += Padding;
		// Process any new groups of 5 ascii-bytes
		Base85::Decode(
			InputBuffer.subspan((EncodedBuffSize - ToRead) / 5, CurRead),
			OutputBuffer);
		if( std::fwrite(
				OutputBuffer.data(), 1, (CurRead / 5) * 4 - Padding,
				Settings.OutputFile)
			!= (CurRead / 5) * 4 - Padding )
		{
			std::fputs("Error writing to output file", stderr);
			munmap(InputBuffer.data(), EncodedBuffSize);
			munmap(OutputBuffer.data(), DecodedBuffSize);
			return EXIT_FAILURE;
		}

		// Set up for next read
		ToRead -= CurRead;
		if( ToRead == 0 )
		{
			ToRead = EncodedBuffSize;
		}
	}
	munmap(InputBuffer.data(), EncodedBuffSize);
	munmap(OutputBuffer.data(), DecodedBuffSize);
	if( std::ferror(Settings.InputFile) )
	{
		std::fputs("Error while reading input file", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

const char* Usage
	= "base85 - Wunkolo <wunkolo@gmail.com>\n"
	  "Usage: base85 [Options]... [File]\n"
	  "       base85 --decode [Options]... [File]\n"
	  "Options:\n"
	  "  -h, --help            Display this help/usage information\n"
	  "  -d, --decode          Decodes incoming base85 into binary bytes\n"
	  "  -i, --ignore-garbage  When decoding, ignores non-base85 characters\n"
	  "  -w, --wrap=Columns    Wrap encoded base85 output within columns\n"
	  "                        Default is `76`. `0` Disables linewrapping\n";

const static struct option CommandOptions[5]
	= {{"decode", optional_argument, nullptr, 'd'},
	   {"ignore-garbage", optional_argument, nullptr, 'i'},
	   {"wrap", optional_argument, nullptr, 'w'},
	   {"help", optional_argument, nullptr, 'h'},
	   {nullptr, no_argument, nullptr, '\0'}};

int main(int argc, char* argv[])
{
	Settings CurSettings = {};
	int      Opt;
	int      OptionIndex;
	while(
		(Opt = getopt_long(argc, argv, "hdiw:", CommandOptions, &OptionIndex))
		!= -1 )
	{
		switch( Opt )
		{
		case 'd':
			CurSettings.Decode = true;
			break;
		case 'i':
			CurSettings.IgnoreInvalid = true;
			break;
		case 'w':
		{
			const std::intmax_t ArgWrap = std::atoi(optarg);
			if( ArgWrap < 0 )
			{
				std::fputs("Invalid wrap width", stderr);
				return EXIT_FAILURE;
			}
			CurSettings.Wrap = ArgWrap;
			break;
		}
		case 'h':
		{
			std::puts(Usage);
			return EXIT_SUCCESS;
		}
		default:
		{
			return EXIT_FAILURE;
		}
		}
	}
	if( optind < argc )
	{
		if( std::strcmp(argv[optind], "-") != 0 )
		{
			CurSettings.InputFile = std::fopen(argv[optind], "rb");
			if( CurSettings.InputFile == nullptr )
			{
				std::fprintf(
					stderr, "Error opening input file: %s\n", argv[optind]);
			}
		}
	}
	return (CurSettings.Decode ? Decode : Encode)(CurSettings);
}

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <sys/mman.h>
#include <unistd.h>
#include <getopt.h>

#include <Base85.hpp>

// Virtual page size of the current system
const static std::size_t ByteBuffSize = sysconf(_SC_PAGE_SIZE);
// Each 4 bytes of input matches up to 5 bytes of output
const static std::size_t AsciiBuffSize = (ByteBuffSize / 4) * 5;

struct Settings
{
	std::FILE* InputFile  = stdin;
	std::FILE* OutputFile = stdout;
	bool Decode           = false;
	bool IgnoreInvalid    = false;
	std::size_t Wrap      = 76;
};

std::size_t WrapWrite(
	const char* Buffer, std::size_t Length, std::size_t WrapWidth,
	std::FILE* OutputFile, std::size_t CurrentColumn = 0
)
{
	if( WrapWidth == 0 )
	{
		return std::fwrite(Buffer, 1, Length, OutputFile);
	}
	for( std::size_t Written = 0; Written < Length; )
	{
		const std::size_t ColumnsRemaining = WrapWidth - CurrentColumn;
		const std::size_t ToWrite = std::min(
			ColumnsRemaining, Length - Written
		);
		if( ToWrite == 0)
		{
			std::fputc('\n',OutputFile);
			CurrentColumn = 0;
		}
		else
		{
			std::fwrite(Buffer + Written, 1, ToWrite, OutputFile);
			CurrentColumn += ToWrite;
			Written += ToWrite;
		}
	}
	return CurrentColumn;
}

bool Encode( const Settings& Settings )
{
	// Every 4 bytes input will map to 5 bytes of output
	std::uint8_t* InputBuffer = static_cast<std::uint8_t*>(
		mmap(
			0, ByteBuffSize,
			PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0
		)
	);
	std::uint64_t* OutputBuffer = static_cast<std::uint64_t*>(
		mmap(
			0, AsciiBuffSize,
			PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0
		)
	);
	std::size_t CurrentColumn = 0;
	std::size_t CurRead = 0;
	while( (CurRead = std::fread(InputBuffer, 1, ByteBuffSize, Settings.InputFile)) )
	{
		Base85::Encode(InputBuffer, OutputBuffer, CurRead);
		CurrentColumn = WrapWrite(
			reinterpret_cast<const char*>(OutputBuffer), (CurRead / 4) * 5,
			Settings.Wrap, Settings.OutputFile, CurrentColumn
		);
	}
	if( std::ferror(Settings.InputFile) )
	{
		std::fputs("Error while reading input file",stderr);
	}
	munmap(InputBuffer,  ByteBuffSize);
	munmap(OutputBuffer, AsciiBuffSize);
	return EXIT_SUCCESS;
}

bool Decode( const Settings& Settings )
{
	// Every 5 bytes of input will map to 4 byte of output
	std::uint64_t* InputBuffer = static_cast<std::uint64_t*>(
		mmap(
			0, AsciiBuffSize,
			PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0
		)
	);
	std::uint8_t* OutputBuffer = static_cast<std::uint8_t*>(
		mmap(
			0, ByteBuffSize,
			PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0
		)
	);

	std::size_t ToRead = AsciiBuffSize;
	// Number of bytes available for actual processing
	std::size_t CurRead = 0;
	// Process paged-sized batches of input in an attempt to have bulk-amounts of
	// conversions going on between calls to `read`
	while(
		(CurRead = std::fread(
			reinterpret_cast<std::uint8_t*>(InputBuffer) + (AsciiBuffSize - ToRead),
			1, ToRead, Settings.InputFile
		))
	)
	{
		// Filter input of all garbage bytes
		if( Settings.IgnoreInvalid )
		{
			CurRead = Base85::Filter(
				reinterpret_cast<std::uint8_t*>(InputBuffer) + (AsciiBuffSize - ToRead),
				CurRead
			);
		}
		// Process any new groups of 5 ascii-bytes
		Base85::Decode(
			InputBuffer + (AsciiBuffSize - ToRead) / 5,
			OutputBuffer, CurRead / 5
		);
		if( std::fwrite(OutputBuffer, 1, (CurRead / 5) * 4, Settings.OutputFile) != (CurRead / 5) * 4 )
		{
			std::fputs("Error writing to output file", stderr);
			munmap(InputBuffer, AsciiBuffSize);
			munmap(OutputBuffer, ByteBuffSize);
			return EXIT_FAILURE;
		}

		// Set up for next read
		ToRead -= CurRead; 
		if( ToRead == 0 )
		{
			ToRead = AsciiBuffSize;
		}
	}
	munmap(InputBuffer, AsciiBuffSize);
	munmap(OutputBuffer, ByteBuffSize);
	if( std::ferror(Settings.InputFile) )
	{
		std::fputs("Error while reading input file",stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

const char* Usage = 
"base85 - Wunkolo <wunkolo@gmail.com>\n"
"Usage: base85 [Options]... [File]\n"
"       base85 --decode [Options]... [File]\n"
"Options:\n"
"  -h, --help            Display this help/usage information\n"
"  -d, --decode          Decodes incoming binary ascii into bytes\n"
"  -i, --ignore-garbage  When decoding, ignores non-base85 characters\n"
"  -w, --wrap=Columns    Wrap encoded binary output within columns\n"
"                        Default is `76`. `0` Disables linewrapping\n";

const static struct option CommandOptions[5] = {
	{ "decode",         optional_argument, nullptr,  'd' },
	{ "ignore-garbage", optional_argument, nullptr,  'i' },
	{ "wrap",           optional_argument, nullptr,  'w' },
	{ "help",           optional_argument, nullptr,  'h' },
	{ nullptr,                no_argument, nullptr, '\0' }
};

int main( int argc, char* argv[] )
{
	Settings CurSettings = {};
	int Opt;
	int OptionIndex;
	while( (Opt = getopt_long(argc, argv, "hdiw:", CommandOptions, &OptionIndex )) != -1 )
	{
		switch( Opt )
		{
		case 'd': CurSettings.Decode = true;            break;
		case 'i': CurSettings.IgnoreInvalid = true;     break;
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
		if( std::strcmp(argv[optind],"-") != 0 )
		{
			CurSettings.InputFile = fopen(argv[optind],"rb");
			if( CurSettings.InputFile == nullptr )
			{
				std::fprintf(
					stderr, "Error opening input file: %s\n", argv[optind]
				);
			}
		}
	}
	return (CurSettings.Decode ? Decode:Encode)(CurSettings);
}

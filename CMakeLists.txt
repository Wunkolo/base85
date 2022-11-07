cmake_minimum_required( VERSION 3.2.2 )
project( base85 )

### Standard
set( CMAKE_CXX_STANDARD 20 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS OFF )

### Verbosity
set( CMAKE_COLOR_MAKEFILE ON )
set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

### Optimizations
if( MSVC )
	add_compile_options( /W4 )
elseif( CMAKE_COMPILER_IS_GNUCXX )
	add_compile_options( -march=native )
	add_compile_options( -Wall )
	add_compile_options( -Wextra )
endif()

### Tests
enable_testing()

# Benchmarks
add_executable(
	base85
	source/Base85.cpp
	source/main.cpp
)
target_include_directories(
	base85
	PRIVATE
	include
)
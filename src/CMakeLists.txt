PROJECT(beam)

cmake_minimum_required(VERSION 2.8)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	SET(CMAKE_CXX_FLAGS "-std=c++11")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	# using Visual Studio C++
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	SET(CMAKE_CXX_FLAGS "-std=c++11")
endif()

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})

SET( LIBRARY_OUTPUT_PATH ${beam_BINARY_DIR}/../lib CACHE PATH
     "Output directory for the beamformer library" )
SET( EXECUTABLE_OUTPUT_PATH 
     ${beam_BINARY_DIR}/../bin CACHE PATH
     "Output directory for the beamformer executables" )

SUBDIRS(beam/lib)
SUBDIRS(beam/exe)

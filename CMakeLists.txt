project(bc-zip)
cmake_minimum_required(VERSION 2.8.7)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
# Compiler flags
message(STATUS ${CMAKE_CXX_COMPILER_ID})

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE CLANG_VERSION)
	if (CLANG_VERSION VERSION_LESS 3.2)
        message(FATAL_ERROR "clang version >= 3.2 required.")
	endif()

	message(STATUS "clang++ found")
	set(CPP_DEF "-std=c++11")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")

	execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
	if (GCC_VERSION VERSION_LESS 4.7)
		message(FATAL_ERROR "gcc version >= 4.6 required; detected: ${GCC_VERSION}.")
	endif()

	message(STATUS "g++ found")
	set(CPP_DEF "-std=c++0x")
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
	execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE ICC_VERSION)
	message(STATUS "Detected ICC version: ${ICC_VERSION}")
	if (ICC_VERSION VERSION_LESS 14)
        message(FATAL_ERROR "icc version >= 14 required.")
	endif()

	message(STATUS "Intel found")
	set(CPP_DEF "-std=c++11")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ansi-alias")
else()
	message(FATAL_ERROR "Compiler not recognized, exiting")
	return()
endif()
set(CMAKE_CXX_FLAGS_FASTDEB "${CPP_DEF} ${CMAKE_CXX_FLAGS_DEBUG} -Wall -O3")
set(CMAKE_CXX_FLAGS_DEBUG "${CPP_DEF} ${CMAKE_CXX_FLAGS_DEBUG} -Wall -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CPP_DEF} ${CMAKE_CXX_FLAGS_RELEASE} -march=native")
set(CMAKE_CXX_FLAGS_TEST "${CPP_DEF} ${CMAKE_CXX_FLAGS_RELEASE} -Wall -O3 -DNDEBUG -march=native")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_VERBOSE_MAKEFILE ON)
endif(NOT CMAKE_BUILD_TYPE)

# Include directory
include_directories("${PROJECT_SOURCE_DIR}/include")
include_directories("${PROJECT_SOURCE_DIR}/ext_libs")

# Divsufsort
set(DIVSUF_BUILD_DIR "difbuild")
add_subdirectory("ext_libs/divsufsort" ${DIVSUF_BUILD_DIR})
include_directories("${PROJECT_BINARY_DIR}/${DIVSUF_BUILD_DIR}/include/")

# Boost
find_package(Boost 1.46 COMPONENTS program_options REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

# Object files
file(GLOB libraries libs/*.c*)
list(APPEND libraries ext_libs/sha1/sha1.cpp)
add_library(bcobjs ${libraries})

# Executable
function(m_link)
	target_link_libraries(${ARGN} -pthread) 
endfunction()

add_library(decompress decompress.cpp)

add_executable(${PROJECT_NAME} bicriteria_compress.cpp  bit_compress.cpp list_gens.cpp  main.cpp)
m_link(${PROJECT_NAME} decompress bcobjs ${Boost_LIBRARIES} divsufsort)

# Include tests
enable_testing()
add_subdirectory(tests)

# Include tools
add_subdirectory(tools)

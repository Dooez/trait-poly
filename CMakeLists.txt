cmake_minimum_required(VERSION 3.16)
project(trp VERSION 0.2.0 LANGUAGES CXX)

add_library(trp_trp INTERFACE)
target_include_directories(trp_trp INTERFACE "include")
target_compile_options(trp_trp INTERFACE -std=c++26 -freflection)
set_target_properties(trp_trp PROPERTIES
		CXX_STANDARD 26
		CXX_STANDARD_REQUIRED ON
)

add_library(trp::trp ALIAS trp_trp)

option(TRP_BUILD_EXAMPLES "Build examples" OFF)
if (TRP_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

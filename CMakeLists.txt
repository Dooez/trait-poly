cmake_minimum_required(VERSION 3.23)
project(trp VERSION 0.2.0 LANGUAGES CXX)

add_library(trp_trp INTERFACE)
target_include_directories(trp_trp INTERFACE "include")
target_compile_features(trp_trp INTERFACE cxx_std_26)
target_compile_options(trp_trp INTERFACE -freflection)
set_target_properties(trp_trp PROPERTIES
		CXX_STANDARD 26
		CXX_STANDARD_REQUIRED ON
)

add_library(trp::trp ALIAS trp_trp)

option(TRP_BUILD_EXAMPLES "Build examples" OFF)
option(TRP_BUILD_TESTING "Build tests" OFF)

if (TRP_BUILD_TESTING)
	enable_testing()
	add_subdirectory(tests)
endif()

if (TRP_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

cmake_minimum_required(VERSION 3.16)
project(trp VERSION 0.0.0 LANGUAGES CXX)

add_executable(trp test_trp.cpp)
add_executable(trp_small test_trp_small.cpp)

set(P2996_INSTALL_PATH "../clang-p2996/install")
set(LLD_PATH "${P2996_INSTALL_PATH}/bin/lld")

add_library(trp_trp INTERFACE)
target_include_directories(trp_trp INTERFACE "include/trp")

function(configure_refl TARGET_NAME)
	target_link_libraries(${TARGET_NAME} PUBLIC "-static" trp_trp)
	target_compile_options(${TARGET_NAME} PRIVATE -std=c++26 -stdlib=libc++ -freflection -fparameter-reflection -fexpansion-statements -O2)
	target_link_options(${TARGET_NAME} PRIVATE
			-fuse-ld=lld
			-stdlib=libc++
			-L "${P2996_INSTALL_PATH}/lib"
			-lc++
			-lc++abi
			-lunwind
	)
endfunction()
configure_refl(trp)
configure_refl(trp_small)

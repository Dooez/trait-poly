cmake_minimum_required(VERSION 3.16)
project(trp VERSION 0.0.0 LANGUAGES CXX)

add_executable(trp test_trp.cpp)
set(P2996_INSTALL_PATH "../clang-p2996/install")
set(LLD_PATH "${P2996_INSTALL_PATH}/bin/lld")

target_link_libraries(trp PUBLIC "-static")
target_compile_options(trp PRIVATE -std=c++26 -stdlib=libc++ -freflection-latest)
target_link_options(trp PRIVATE
			-fuse-ld=lld
			-stdlib=libc++
			-L "${P2996_INSTALL_PATH}/lib"
			-lc++
			-lc++abi
			-lunwind
)

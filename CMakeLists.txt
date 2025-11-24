cmake_minimum_required(VERSION 3.16)
project(reflectbolt VERSION 0.0.0 LANGUAGES CXX)

add_executable(bolt reflectbolt.cpp)
set(P2996_INSTALL_PATH "/home/marifey/repos/clang-p2996/install")
set(LLD_PATH "${P2996_INSTALL_PATH}/bin/lld")

target_link_libraries(bolt PUBLIC "-static")
target_compile_options(bolt PRIVATE -std=c++26 -stdlib=libc++ -freflection-latest)
target_link_options(bolt PRIVATE
			-fuse-ld=lld
			-stdlib=libc++
			-L "${P2996_INSTALL_PATH}/lib"
			-lc++
			-lc++abi
			-lunwind
)

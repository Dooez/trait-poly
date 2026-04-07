

set(P2996_INSTALL_PATH "../clang-p2996/install")
set(LLD_PATH "${P2996_INSTALL_PATH}/bin/lld")

add_executable(trp test_trp.cpp)
add_executable(trp_small test_trp_small.cpp)
add_executable(trp_cv_supertraits cv_supertraits.cpp)

function(configure_refl TARGET_NAME)
	target_link_libraries(${TARGET_NAME} PUBLIC "-static" trp::trp)
	target_compile_options(${TARGET_NAME} PRIVATE -stdlib=libc++ -fparameter-reflection -fexpansion-statements -fconstexpr-steps=2000000 -O2)
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
configure_refl(trp_cv_supertraits)

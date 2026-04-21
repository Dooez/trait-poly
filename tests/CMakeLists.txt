get_filename_component(_trp_compiler_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
get_filename_component(_trp_toolchain_root "${_trp_compiler_bin_dir}" DIRECTORY)
file(GLOB _trp_runtime_candidates
	"${_trp_toolchain_root}/lib/libc++abi.so*"
	"${_trp_toolchain_root}/lib/*/libc++abi.so*")

if (_trp_runtime_candidates)
	list(GET _trp_runtime_candidates 0 _trp_runtime_library)
	get_filename_component(TRP_RUNTIME_LIB_DIR "${_trp_runtime_library}" DIRECTORY)
endif()

function(add_trp_test target source)
	add_executable(${target} ${source})
	target_link_libraries(${target} PRIVATE trp::trp)
	add_test(NAME ${target} COMMAND ${target})
	if (TRP_RUNTIME_LIB_DIR)
		set_target_properties(${target} PROPERTIES BUILD_RPATH "${TRP_RUNTIME_LIB_DIR}")
		set_property(TEST ${target} PROPERTY ENVIRONMENT
			"LD_LIBRARY_PATH=${TRP_RUNTIME_LIB_DIR}:$ENV{LD_LIBRARY_PATH}")
	endif()
endfunction()

add_trp_test(trp_test_default_trait_impls default_trait_impl_matrix.cpp)

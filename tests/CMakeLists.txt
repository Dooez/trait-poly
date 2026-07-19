execute_process(
	COMMAND "${CMAKE_CXX_COMPILER}" --print-file-name=libc++abi.so
	OUTPUT_VARIABLE runtime_library
	OUTPUT_STRIP_TRAILING_WHITESPACE)

if (EXISTS "${runtime_library}")
	get_filename_component(CMAKE_BUILD_RPATH "${runtime_library}" DIRECTORY)
endif()

function(add_trp_test name source)
	set(target "test_${name}")
	add_executable(${target} "${source}")
	target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
	target_link_libraries(${target} PRIVATE trp::trp)
	add_test(NAME ${target} COMMAND ${target})
endfunction()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompileFailureTest.cmake")

foreach(source IN ITEMS
	supertraits
	dyn_trait_ref
	cv_qualification
	reference_adapter_conformance
	reference_adapters
	reference_inheritance
	reference_qualification
	unique_trait_ptr_ownership
	shared_trait_ptr_ownership
	trait_variant
	trait_variant_lifecycle
	overload_resolution
	requirements/overload_limitations
	requirements/overrides
	requirements/reference_qualification
	requirements/strengthening)
	string(REPLACE "requirements/" "req_" name "${source}")
	add_trp_test(${name} "${source}.cpp")
endforeach()

add_trp_compile_failure(
	req_reference_default_requires_arguments
	"requirements/reference_default_requires_arguments.cpp"
	"Exact reference qualification requirements can only be enabled")

add_trp_compile_failure(
	req_reference_annotation_requires_arguments
	"requirements/reference_annotation_requires_arguments.cpp"
	"Exact reference signature requirement must include exact arguments")

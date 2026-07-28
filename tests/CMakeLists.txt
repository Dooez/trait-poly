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
	target_compile_definitions(${target} PRIVATE TRP_DEV)
	target_link_libraries(${target} PRIVATE trp::trp)
	add_test(NAME ${target} COMMAND ${target})
endfunction()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompileFailureTest.cmake")

add_trp_test(traits "traits/traits.cpp")
add_trp_test(dyn_trait_ref "dyn_trait_ref/dyn_trait_ref.cpp")
add_trp_test(implementations "implementations/implementations.cpp")
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	target_compile_definitions(test_implementations PRIVATE TRP_TEST_STATIC_TEMPLATE)
endif()
add_trp_test(noexcept "noexcept.cpp")
add_trp_test(overloads "overloads/overloads.cpp")
add_trp_test(cvts_trait_ref "cvts_trait_ref.cpp")
add_trp_test(trait_ptr "trait_ptr/trait_ptr.cpp")
add_trp_test(trait_variant "trait_variant/trait_variant.cpp")

# These macros change method_signature_requirements_t, so each configuration
# must stay in a separate executable to avoid violating ODR.
add_trp_test(requirements "requirements/requirements.cpp")
add_trp_test(requirements_exact_arguments "requirements/exact_arguments.cpp")
add_trp_test(requirements_overrides "requirements/overrides.cpp")
add_trp_test(requirements_exact_references "requirements/exact_references.cpp")

add_trp_compile_failure(
	req_reference_default_requires_arguments
	"requirements/reference_default_requires_arguments.cpp"
	"Exact reference qualification requirements can only be enabled")

add_trp_compile_failure(
	req_reference_annotation_requires_arguments
	"requirements/reference_annotation_requires_arguments.cpp"
	"Exact reference signature requirement must include exact arguments")

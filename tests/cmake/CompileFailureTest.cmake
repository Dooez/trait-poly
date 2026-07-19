function(add_trp_compile_failure name source diagnostic)
	set(test "test_${name}")
	set(target "${test}_compile")
	add_executable(${target} EXCLUDE_FROM_ALL "${source}")
	target_link_libraries(${target} PRIVATE trp::trp)
	add_test(NAME ${test}
		COMMAND ${CMAKE_COMMAND}
			-DTRP_COMPILE_FAILURE_RUN=ON
			"-DTRP_BUILD_DIR=${CMAKE_BINARY_DIR}"
			"-DTRP_COMPILE_TARGET=${target}"
			"-DTRP_EXPECTED_DIAGNOSTIC=${diagnostic}"
			-P "${CMAKE_CURRENT_FUNCTION_LIST_FILE}")
	set_property(TEST ${test} PROPERTY RUN_SERIAL TRUE)
endfunction()

if (TRP_COMPILE_FAILURE_RUN)
	execute_process(
		COMMAND "${CMAKE_COMMAND}" --build "${TRP_BUILD_DIR}" --target "${TRP_COMPILE_TARGET}"
		RESULT_VARIABLE result
		OUTPUT_VARIABLE stdout
		ERROR_VARIABLE stderr)

	if (result EQUAL 0)
		message(FATAL_ERROR "Compilation unexpectedly succeeded")
	endif()

	if (NOT "${stdout}\n${stderr}" MATCHES "${TRP_EXPECTED_DIAGNOSTIC}")
		message(FATAL_ERROR "Expected diagnostic not found:\n${stdout}\n${stderr}")
	endif()
endif()

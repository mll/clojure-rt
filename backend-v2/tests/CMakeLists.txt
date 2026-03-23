set(COMMON_TEST_SOURCES 
    ../runtime/tests/TestTools.c
    codegen/runtime_helpers.c
)

set(TEST_MODULES
    tools/EdnParser_test
    codegen/Overflow_test
    codegen/IfNode_test
    codegen/StaticCallType_test
    codegen/StaticCallNode_test
    codegen/StaticFieldNode_test
    codegen/JITArithmetic_test
    codegen/MathIntrinsics_test
    codegen/DoubleAddRepro_test
    codegen/NoThrow_test
    codegen/DoNode_test
    codegen/LetNode_test
    codegen/MathIntrinsicsRepro_test
    codegen/InstanceCallNode_test
    codegen/InstanceCallIC_test
    codegen/VoidInvokeRepro_test
    codegen/DCE_test
    codegen/O3_DebugInfo_test
    codegen/AsyncStackTrace_test
    VarUAF_repro_test
    JITSafety_test
)

foreach(TEST_MODULE ${TEST_MODULES})
    string(REPLACE "/" "_" SAFE_TEST_MODULE ${TEST_MODULE})
    set(EXEC_NAME backend_${SAFE_TEST_MODULE})
    add_executable(${EXEC_NAME} ${TEST_MODULE}.cpp ${COMMON_TEST_SOURCES})
    target_link_libraries(${EXEC_NAME} PUBLIC backend_lib runtime ${CMOCKA_LIBRARIES} ${GMP_LIBRARY})
    
    # On Linux, export symbols so the JIT can find runtime functions
    target_link_options(${EXEC_NAME} PRIVATE -rdynamic)

    add_test(
        NAME ${EXEC_NAME} 
        COMMAND ${EXEC_NAME}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(${EXEC_NAME} PROPERTIES LABELS "Unit")
endforeach()

# --- Frontend Tests ---
find_program(LEIN_EXECUTABLE lein)
if(LEIN_EXECUTABLE)
    add_test(
        NAME frontend_tests
        COMMAND ${LEIN_EXECUTABLE} test
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/../frontend
    )
    set_tests_properties(frontend_tests PROPERTIES LABELS "Frontend")
else()
    message(WARNING "Leiningen not found, skipping frontend tests")
endif()

set(COMMON_TEST_SOURCES 
    ../runtime/tests/TestTools.c
)

set(TEST_MODULES
    EdnParser_test
)

foreach(TEST_MODULE ${TEST_MODULES})
    set(EXEC_NAME backend_${TEST_MODULE})
    add_executable(${EXEC_NAME} ${TEST_MODULE}.cpp ${COMMON_TEST_SOURCES})
    target_link_libraries(${EXEC_NAME} PUBLIC backend_lib runtime ${CMOCKA_LIBRARIES} ${GMP_LIBRARY})
    
    add_test(
        NAME ${EXEC_NAME} 
        COMMAND ${EXEC_NAME}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(${EXEC_NAME} PROPERTIES LABELS "Unit")
endforeach()

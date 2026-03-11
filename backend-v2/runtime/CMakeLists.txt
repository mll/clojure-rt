cmake_minimum_required(VERSION 3.26)
project(runtime)


if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g -fsanitize=address -fno-omit-frame-pointer -Wall -Wextra -Wstrict-aliasing -Wno-unused-parameter -fexceptions")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -fexceptions")

set(CMAKE_C_FLAGS_DEBUG "-g -fsanitize=address -fno-omit-frame-pointer -Wall -Wextra -Wstrict-aliasing -Wno-unused-parameter -fexceptions")
set(CMAKE_C_FLAGS_RELEASE "-Ofast -fexceptions")

set(SOURCES
    Transient.c
    Object.c 
    Integer.c 
    PersistentList.c 
    PersistentVector.c 
    PersistentVectorIterator.c 
    PersistentVectorNode.c 
    String.c 
    Double.c 
    Nil.c 
    Boolean.c 
    Symbol.c 
    ConcurrentHashMap.c 
    Keyword.c 
    Function.c
    BigInteger.c
    Ratio.c
    Class.c
    RuntimeInterface.c
    PersistentArrayMap.c)

add_library(runtime STATIC ${SOURCES})

find_path(GMP_INCLUDE_DIRS NAMES gmp.h)
find_library(GMP_LIBRARY NAMES gmp libgmp)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GMP DEFAULT_MSG GMP_INCLUDE_DIRS GMP_LIBRARY)
mark_as_advanced(GMP_INCLUDE_DIRS GMP_LIBRARY)
include_directories(${GMP_INCLUDE_DIRS})

enable_testing()

find_package(Cmocka REQUIRED)
include_directories(${CMOCKA_INCLUDE_DIRS})

set(COMMON_TEST_SOURCES 
    tests/TestTools.c
    tests/ExceptionsMock.c
)

set(TEST_MODULES
    ConcurrentHashMap_test
    PersistentList_test
    PersistentVector_test
    String_test
    Keyword_test
    Symbol_test
    PersistentArrayMap_test
    BigInteger_test
    Ratio_test
    Boolean_test
)

foreach(TEST_MODULE ${TEST_MODULES})
    set(EXEC_NAME ${TEST_MODULE})
    add_executable(${EXEC_NAME} tests/${TEST_MODULE}.c ${COMMON_TEST_SOURCES})
    target_link_libraries(${EXEC_NAME} PUBLIC runtime ${GMP_LIBRARY} ${CMOCKA_LIBRARIES})
    
    add_test(
        NAME ${EXEC_NAME} 
        COMMAND ${EXEC_NAME}
        LABELS "Unit"
    )
endforeach()

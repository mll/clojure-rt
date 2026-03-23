cmake_minimum_required(VERSION 3.26)
project(runtime)


if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g -fsanitize=address -fno-omit-frame-pointer -Wall -Wextra -Wstrict-aliasing -Wno-unused-parameter -fexceptions")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -fexceptions")

set(CMAKE_C_FLAGS_DEBUG "-g -fsanitize=address -fno-omit-frame-pointer -Wall -Wextra -Wstrict-aliasing -Wno-unused-parameter -fexceptions")
set(CMAKE_C_FLAGS_RELEASE "-Ofast -fexceptions")

add_compile_definitions(BUILD_TYPE="${CMAKE_BUILD_TYPE}")

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
    PersistentArrayMap.c
    Var.c
    Numbers.c
    JITSafety.c)

add_library(runtime STATIC ${SOURCES})
target_compile_options(runtime PRIVATE $<$<PLATFORM_ID:Linux>:-femulated-tls>)

# GMP Detection
find_path(GMP_INCLUDE_DIRS NAMES gmp.h)
find_library(GMP_LIBRARY_PATH NAMES gmp libgmp)
if(GMP_LIBRARY_PATH)
    set(GMP_LIBRARY ${GMP_LIBRARY_PATH})
else()
    set(GMP_LIBRARY "")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GMP DEFAULT_MSG GMP_INCLUDE_DIRS GMP_LIBRARY)
mark_as_advanced(GMP_INCLUDE_DIRS GMP_LIBRARY)
include_directories(${GMP_INCLUDE_DIRS})

########################################################################
# BITCODE GENERATION FOR JIT INTROSPECTION
# ##############################################################################

if(NOT LLVM_TOOLS_BINARY_DIR AND LLVM_DIR)
    # If LLVM_DIR is /prefix/lib/cmake/llvm, then tools are in /prefix/bin
    get_filename_component(LLVM_PREFIX "${LLVM_DIR}/../../.." ABSOLUTE)
    set(LLVM_TOOLS_BINARY_DIR "${LLVM_PREFIX}/bin")
endif()

# 1. Find the required LLVM tools
# We use PATHS to ensure we use the tools from the SAME LLVM version as the JIT
find_program(CLANG_EXECUTABLE clang 
             PATHS ${LLVM_TOOLS_BINARY_DIR}
             NO_DEFAULT_PATH
             REQUIRED)
find_program(LLVM_LINK_EXECUTABLE llvm-link 
             PATHS ${LLVM_TOOLS_BINARY_DIR}
             NO_DEFAULT_PATH
             REQUIRED)

set(BITCODE_OUTPUTS "")

set(EMULATED_TLS_FLAG "-fno-emulated-tls")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(EMULATED_TLS_FLAG "-femulated-tls")
endif()

# 2. Compile each source file to an individual .bc file
foreach(src ${SOURCES})
    get_filename_component(basename ${src} NAME_WE)
    set(bc_output "${CMAKE_CURRENT_BINARY_DIR}/${basename}.bc")
    
    add_custom_command(
        OUTPUT ${bc_output}
        COMMAND ${CLANG_EXECUTABLE} -emit-llvm -c 
                ${CMAKE_CURRENT_SOURCE_DIR}/${src}
                -iquote ${CMAKE_CURRENT_SOURCE_DIR} -I${GMP_INCLUDE_DIRS}
                -O3 -fPIC -fexceptions ${EMULATED_TLS_FLAG} 
                -o ${bc_output}
        DEPENDS ${src}
        COMMENT "Generating LLVM Bitcode for ${src}"
    )
    list(APPEND BITCODE_OUTPUTS ${bc_output})
endforeach()

# 3. Link all bitcode files into one uberIR
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/runtime_uber.bc
    COMMAND ${LLVM_LINK_EXECUTABLE} ${BITCODE_OUTPUTS} -o ${CMAKE_CURRENT_BINARY_DIR}/runtime_uber.bc
    DEPENDS ${BITCODE_OUTPUTS}
    COMMENT "Linking all bitcode into runtime_uber.bc"
)

# 4. Create a target to trigger the build
add_custom_target(runtime_ir ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/runtime_uber.bc)

########################################################################


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
    Integer_test
    Ratio_test
    Boolean_test
    Var_test
    Var_Race_test
    RefcountPerf_test
    Numbers_test
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

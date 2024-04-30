cmake_minimum_required(VERSION 3.26)
project(runtime)

#set(CMAKE_OSX_ARCHITECTURES "arm64")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

#set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast")

#set(CMAKE_C_FLAGS "-Wall -Wextra")
set(CMAKE_C_FLAGS_DEBUG "-g")
set(CMAKE_C_FLAGS_RELEASE "-Ofast")

set(SOURCES
    Transient.c
    Object.c 
    Integer.c 
    PersistentList.c 
    PersistentVector.c 
    PersistentVectorNode.c 
    String.c 
    Double.c 
    Nil.c 
    Boolean.c 
    Symbol.c 
    Class.c
    Deftype.c
    ConcurrentHashMap.c 
    Interface.c 
    Keyword.c 
    Function.c
    Var.c
    BigInteger.c
    Ratio.c
    PersistentArrayMap.c)

add_library(runtime STATIC ${SOURCES})

find_path(GMP_INCLUDE_DIRS NAMES gmp.h)
find_library(GMP_LIBRARY NAMES gmp libgmp)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GMP DEFAULT_MSG GMP_INCLUDE_DIRS GMP_LIBRARY)
mark_as_advanced(GMP_INCLUDE_DIRS GMP_LIBRARY)
include_directories(${GMP_INCLUDE_DIRS})

add_executable(runtime-tests runtime-tests.cpp)

target_link_libraries(runtime-tests PUBLIC runtime ${GMP_LIBRARY})

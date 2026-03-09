cmake_minimum_required(VERSION 3.26)
project(clojure-rt)


message(STATUS "CMAKE_OSX_ARCHITECTURES: ${CMAKE_OSX_ARCHITECTURES}")
message(STATUS "CMAKE_APPLE_SILICON_PROCESSOR: ${CMAKE_APPLE_SILICON_PROCESSOR}")
message(STATUS "CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "CMAKE_HOST_SYSTEM_PROCESSOR: ${CMAKE_HOST_SYSTEM_PROCESSOR}")

#set(CMAKE_OPTIMIZE_DEPENDENCIES TRUE)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

#set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

#set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -fsanitize=address -fno-omit-frame-pointer -Wall -Wextra -Wstrict-aliasing -Wno-unused-parameter -fstandalone-debug")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast")
set(CMAKE_CXX_STANDARD 20)
add_compile_definitions(BUILD_TYPE="${CMAKE_BUILD_TYPE}")

if(APPLE)
    set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym")
endif()

add_subdirectory(runtime)

# Use LLVM's new approach for finding packages.
# LLVM 20.1 is very new, ensure your system has LLVMConfig.cmake for it.
find_package(LLVM 20.1 CONFIG REQUIRED) # Added REQUIRED for stricter checking.

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# --- START OF CRITICAL CHANGES FOR LINKING ---

# 1. Define the LLVM components needed for a JIT targeting AArch64.
# These components map directly to the missing symbols in your error log.
# 'Native' is usually replaced by specific component names in modern LLVM.
# LLVMTarget requires the specific architecture libraries to be initialized.
set(LLVM_COMPONENTS
    # Core LLVM components (e.g., Instruction, BasicBlock, Module, LLVMContext, etc.)
    Core
    # Support for the JIT engine (ORC)
    ORCJIT
    JITLink
    ExecutionEngine
    RuntimeDyld
    # AArch64 components for assembly parsing/printing and code generation
    AArch64
    # Utility libraries for LLVM passes, etc. (often implicitly included, but good to be explicit)
    Analysis
    Passes  
)

# 2. Map the component names to the actual library files.
# We map to the dynamic library (LLVM) instead of static components to prevent
# ODR violations between our ASAN-instrumented std::string and LLVM. 
set(LLVM_LIBS LLVM)
message(STATUS "LLVM Components required: ${LLVM_COMPONENTS}")
message(STATUS "LLVM Libraries to link: ${LLVM_LIBS}")

# 3. Add necessary definitions and include paths from LLVM package.
include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# --- END OF CRITICAL CHANGES FOR LINKING ---

set(BACKEND_SOURCES
      bridge/Exceptions.cpp
      types/ObjectTypeSet.cpp
      tools/ThreadPool.cpp
      jit/JITEngine.cpp
      state/ThreadsafeCompilerState.cpp
      tools/EdnParser.cpp
      codegen/ValueEncoder.cpp
      codegen/CodeGen.cpp
      codegen/LLVMTypes.cpp
      codegen/DynamicConstructor.cpp
      codegen/invoke/InvokeManager.cpp
      codegen/MemoryManagement.cpp
      codegen/ops/ConstNode.cpp
      codegen/ops/QuoteNode.cpp
      codegen/ops/VectorNode.cpp       
      codegen/ops/MapNode.cpp)


find_package(Protobuf REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ../model/bytecode.proto)

add_library(backend_lib STATIC ${BACKEND_SOURCES} ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(backend_lib PUBLIC runtime ${LLVM_LIBS} ${Protobuf_LIBRARIES} ${GMP_LIBRARY})

add_executable(clojure-rt main.cpp)
target_link_libraries(clojure-rt PUBLIC backend_lib)

find_path(GMP_INCLUDE_DIRS NAMES gmp.h)
find_library(GMP_LIBRARY NAMES gmp libgmp)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GMP DEFAULT_MSG GMP_INCLUDE_DIRS GMP_LIBRARY)
mark_as_advanced(GMP_INCLUDE_DIRS GMP_LIBRARY)
include_directories(${GMP_INCLUDE_DIRS})

enable_testing()
find_package(Cmocka REQUIRED)
include_directories(${CMOCKA_INCLUDE_DIRS})

add_subdirectory(tests)

if(APPLE)
    find_program(DSYMUTIL_TOOL dsymutil)
    if(DSYMUTIL_TOOL)
        add_custom_command(TARGET clojure-rt POST_BUILD
            COMMAND ${DSYMUTIL_TOOL} $<TARGET_FILE:clojure-rt>
            COMMENT "Generowanie pliku .dSYM dla clojure-rt..."
            VERBATIM
        )
    endif()

    # Opcjonalnie: jeśli chcesz, aby binarka po wyciągnięciu symboli była mniejsza:
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_custom_command(TARGET clojure-rt POST_BUILD
            COMMAND strip -x $<TARGET_FILE:clojure-rt>
            COMMENT "Stripping binarki (usuwanie zbędnych symboli)..."
            DEPENDS clojure-rt
        )
    endif()
endif()


# The linker options block is commented out, but if you re-enable it, ensure it comes after add_executable.
# if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
#  target_link_options(clojure-rt PRIVATE -Wl,-dead_strip -Wl,-x)
# elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
#  target_link_options(clojure-rt PRIVATE -Wl,--gc-sections -Wl,--strip-all)
# else()
#  message(WARNING "Unsupported platform for linker optimizations")
# endif()


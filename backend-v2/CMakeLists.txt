cmake_minimum_required(VERSION 3.26)
project(clojure-rt)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


if(APPLE)
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64")
    endif()
    set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym")
endif()


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

add_subdirectory(runtime)

# --- Dependency Discovery ---

# Helper function to find Homebrew prefix on macOS
macro(find_homebrew_prefix PREFIX_VAR FORMULA_NAME)
    if(APPLE AND NOT ${PREFIX_VAR})
        execute_process(
            COMMAND brew --prefix ${FORMULA_NAME}
            RESULT_VARIABLE BREW_RESULT
            OUTPUT_VARIABLE BREW_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(BREW_RESULT EQUAL 0)
            set(${PREFIX_VAR} "${BREW_PREFIX}")
        endif()
    endif()
endmacro()

# 1. LLVM Detection
# We prefer LLVM 20.x for API stability (getContext() availability).
find_package(LLVM 20.1 CONFIG QUIET)

# Fallback for macOS Homebrew if not found in standard paths
if(NOT LLVM_FOUND AND APPLE)
    find_homebrew_prefix(LLVM_PREFIX llvm@20)
    if(LLVM_PREFIX)
        set(LLVM_DIR "${LLVM_PREFIX}/lib/cmake/llvm")
        find_package(LLVM 20.1 CONFIG REQUIRED)
    endif()
endif()

# Final check for LLVM
if(NOT LLVM_FOUND)
    find_package(LLVM REQUIRED CONFIG)
endif()

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
      codegen/invoke/InvokeManager_Math.cpp
      codegen/invoke/InvokeManager_Cmp.cpp

      codegen/MemoryManagement.cpp
      codegen/ops/ConstNode.cpp
      codegen/ops/QuoteNode.cpp
      codegen/ops/VectorNode.cpp       
      codegen/ops/MapNode.cpp
      codegen/ops/StaticCallNode.cpp
      codegen/ops/StaticFieldNode.cpp
      codegen/ops/IfNode.cpp
      codegen/ops/DoNode.cpp
      codegen/ops/DefNode.cpp
      codegen/ops/TheVarNode.cpp
      codegen/ops/VarNode.cpp
      codegen/ops/WithMetaNode.cpp)


# 2. Protobuf Detection
find_package(Protobuf CONFIG QUIET)
if(NOT Protobuf_FOUND)
    find_package(Protobuf REQUIRED)
endif()

include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Use the modern protobuf_generate command if available, fallback to protobuf_generate_cpp
if(COMMAND protobuf_generate)
    protobuf_generate(LANGUAGE cpp OUT_VAR PROTO_SRCS PROTOC_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}" APPEND_PATH PROTOS ../model/bytecode.proto)
else()
    protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ../model/bytecode.proto)
endif()

add_library(backend_lib STATIC ${BACKEND_SOURCES} ${PROTO_SRCS} ${PROTO_HDRS})
# Use target-based linking for Protobuf to handle Abseil dependencies automatically
if(TARGET protobuf::libprotobuf)
    set(PROTOBUF_LINK_TARGET protobuf::libprotobuf)
else()
    set(PROTOBUF_LINK_TARGET ${Protobuf_LIBRARIES})
endif()

target_link_libraries(backend_lib PUBLIC runtime ${LLVM_LIBS} ${PROTOBUF_LINK_TARGET} ${GMP_LIBRARY})

add_executable(clojure-rt main.cpp)
target_link_libraries(clojure-rt PUBLIC backend_lib)

# On Linux, export symbols so the JIT can find runtime functions
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(clojure-rt PRIVATE -rdynamic)
endif()

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


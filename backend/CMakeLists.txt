cmake_minimum_required(VERSION 3.26)
project(clojure-rt)


message(STATUS "CMAKE_OSX_ARCHITECTURES: ${CMAKE_OSX_ARCHITECTURES}")
message(STATUS "CMAKE_APPLE_SILICON_PROCESSOR: ${CMAKE_APPLE_SILICON_PROCESSOR}")
message(STATUS "CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "CMAKE_HOST_SYSTEM_PROCESSOR: ${CMAKE_HOST_SYSTEM_PROCESSOR}")

set(CMAKE_OSX_ARCHITECTURES "arm64")
#set(CMAKE_OPTIMIZE_DEPENDENCIES TRUE)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

#set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

#set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -fsanitize=address -fno-omit-frame-pointer -Wall -Wextra -Wstrict-aliasing -Wno-unused-parameter")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast")
set(CMAKE_CXX_STANDARD 17)

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
# This function is the standard way to get library names in modern LLVM CMake builds.
llvm_map_components_to_libnames(LLVM_LIBS ${LLVM_COMPONENTS})
message(STATUS "LLVM Components required: ${LLVM_COMPONENTS}")
message(STATUS "LLVM Libraries to link: ${LLVM_LIBS}")

# 3. Add necessary definitions and include paths from LLVM package.
include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# --- END OF CRITICAL CHANGES FOR LINKING ---

set(SOURCES
      main.cpp
      codegen.cpp
      jit/jit.cpp
      llvm-passes/RetainRelease.cpp
      ops/BindingNode.cpp
      ops/CaseNode.cpp
      ops/CaseTestNode.cpp
      ops/CaseThenNode.cpp
      ops/CatchNode.cpp
      ops/ConstNode.cpp
      ops/DefNode.cpp
      ops/DeftypeNode.cpp
      ops/DoNode.cpp
      ops/FnMethodNode.cpp
      ops/FnNode.cpp
      ops/HostInteropNode.cpp
      ops/IfNode.cpp
      ops/ImportNode.cpp
      ops/InstanceCallNode.cpp
      ops/InstanceFieldNode.cpp
      ops/InvokeNode.cpp
      ops/IsInstanceNode.cpp
      ops/KeywordInvokeNode.cpp
      ops/LetNode.cpp
      ops/LetfnNode.cpp
      ops/LocalNode.cpp
      ops/LoopNode.cpp
      ops/MapNode.cpp
      ops/MethodNode.cpp
      ops/MonitorEnterNode.cpp
      ops/MonitorExitNode.cpp
      ops/MutateSetNode.cpp
      ops/NewNode.cpp
      ops/PrimInvokeNode.cpp
      ops/ProtocolInvokeNode.cpp
      ops/QuoteNode.cpp
      ops/RecurNode.cpp
      ops/ReifyNode.cpp
      ops/SetNode.cpp
      ops/StaticCallNode.cpp
      ops/StaticFieldNode.cpp
      ops/TheVarNode.cpp
      ops/ThrowNode.cpp
      ops/TryNode.cpp
      ops/VarNode.cpp
      ops/VectorNode.cpp
      ops/WithMetaNode.cpp
      static/Numbers.cpp
      static/Class.cpp
      static/Deftype.cpp
      static/String.cpp
      static/Var.cpp
      RuntimeInterop.cpp
      ObjectTypeSet.cpp
      static/Utils.cpp
      static/Vector.cpp
      jit/Materialisation.cpp
      jit/ASTLayer.cpp
      invocation/DynamicInvoke.cpp
      invocation/ReturnTypeInference.cpp
      invocation/StaticInvoke.cpp
      invocation/SpecialiseDynamicFn.cpp
      invocation/ClosedOvers.cpp
      invocation/VarArgs.cpp
      BuildFunction.cpp
      Programme.cpp)


find_package(Protobuf REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ../model/bytecode.proto)
add_executable(clojure-rt ${SOURCES} ${PROTO_SRCS} ${PROTO_HDRS})

find_path(GMP_INCLUDE_DIRS NAMES gmp.h)
find_library(GMP_LIBRARY NAMES gmp libgmp)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GMP DEFAULT_MSG GMP_INCLUDE_DIRS GMP_LIBRARY)
mark_as_advanced(GMP_INCLUDE_DIRS GMP_LIBRARY)
include_directories(${GMP_INCLUDE_DIRS})

# Update the target_link_libraries call to use the newly defined LLVM_LIBS variable
# which now contains all necessary LLVM components.
target_link_libraries(clojure-rt PUBLIC runtime ${LLVM_LIBS} ${Protobuf_LIBRARIES} ${GMP_LIBRARY})

# The linker options block is commented out, but if you re-enable it, ensure it comes after add_executable.
# if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
#  target_link_options(clojure-rt PRIVATE -Wl,-dead_strip -Wl,-x)
# elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
#  target_link_options(clojure-rt PRIVATE -Wl,--gc-sections -Wl,--strip-all)
# else()
#  message(WARNING "Unsupported platform for linker optimizations")
# endif()


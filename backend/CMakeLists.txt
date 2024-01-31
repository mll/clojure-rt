cmake_minimum_required(VERSION 3.26)
project(clojure-rt)

#set(CMAKE_OSX_ARCHITECTURES "arm64")
#set(CMAKE_OPTIMIZE_DEPENDENCIES TRUE)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

#set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

#set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast")
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(runtime)
find_package(LLVM 16.0 CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")


set(SOURCES
     main.cpp 
     codegen.cpp 
     jit/jit.cpp 
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
     RuntimeInterop.cpp
     ObjectTypeSet.cpp
     static/Utils.cpp
     static/Vector.cpp
     jit/Materialisation.cpp
     jit/ASTLayer.cpp
     DynamicRuntime.cpp
     DynamicDispatch.cpp
     Programme.cpp) 

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

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

llvm_map_components_to_libnames(llvm_libs core orcjit native)
message(STATUS "LLVM libs: ${llvm_libs}")
target_link_libraries(clojure-rt PUBLIC runtime ${llvm_libs} ${Protobuf_LIBRARIES} ${GMP_LIBRARY})


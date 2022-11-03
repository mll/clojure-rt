
CC = arch -arm64 clang
CCP = arch -arm64 clang++
AR = arch -arm64 ar
ARFLAGS = rcs
MYFLAGSOPT = -Ofast  
MYFLAGS = -g 
CPPFLAGS = -std=c++14  
CFLAGS =  -Wall $(MYFLAGS) -I/opt/homebrew/include
PROFILE = -Ofast 
LIBS = -L/opt/homebrew/lib -lprotobuf

OBJFILES = main.o runtime/runtime.a bytecode.pb.o codegen.o jit.o BindingNode.o CaseNode.o CaseTestNode.o CaseThenNode.o CatchNode.o ConstNode.o DefNode.o DeftypeNode.o DoNode.o \
FnMethodNode.o FnNode.o HostInteropNode.o IfNode.o ImportNode.o InstanceCallNode.o InstanceFieldNode.o InvokeNode.o IsInstanceNode.o KeywordInvokeNode.o LetNode.o LetfnNode.o \
LocalNode.o LoopNode.o MapNode.o MethodNode.o MonitorEnterNode.o MonitorExitNode.o MutateSetNode.o NewNode.o PrimInvokeNode.o ProtocolInvokeNode.o QuoteNode.o RecurNode.o ReifyNode.o \
SetNode.o StaticCallNode.o StaticFieldNode.o TheVarNode.o ThrowNode.o TryNode.o VarNode.o VectorNode.o WithMetaNode.o Numbers.o RuntimeInterop.o ObjectTypeSet.o Utils.o libtcmalloc_and_profiler.a 
           
LLVM_LINKER_FLAGS = $(shell llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native)
LLVM_COMPILE_FLAGS = $(shell llvm-config --cxxflags --system-libs)

default: all

all: 
	$(MAKE) clojure-rt
runtime/runtime.a:
	$(MAKE) -C runtime  
clojure-rt: $(OBJFILES)
	$(CCP) -o clojure-rt  $(LIBS) $(LLVM_LINKER_FLAGS) $(OBJFILES) 
clean:
	rm -f *.o
	$(MAKE) -C runtime -f Makefile clean
main.o: main.cpp 
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) main.cpp
bytecode.pb.o: protobuf/bytecode.pb.cc protobuf/bytecode.pb.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) protobuf/bytecode.pb.cc
codegen.o: codegen.cpp 
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) codegen.cpp
jit.o: jit.cpp jit.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) jit.cpp

BindingNode.o: ops/BindingNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/BindingNode.cpp
CaseNode.o: ops/CaseNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/CaseNode.cpp
CaseTestNode.o: ops/CaseTestNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/CaseTestNode.cpp
CaseThenNode.o: ops/CaseThenNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/CaseThenNode.cpp
CatchNode.o: ops/CatchNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/CatchNode.cpp
ConstNode.o: ops/ConstNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/ConstNode.cpp
DefNode.o: ops/DefNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/DefNode.cpp
DeftypeNode.o: ops/DeftypeNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/DeftypeNode.cpp
DoNode.o: ops/DoNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/DoNode.cpp
FnMethodNode.o: ops/FnMethodNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/FnMethodNode.cpp
FnNode.o: ops/FnNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/FnNode.cpp
HostInteropNode.o: ops/HostInteropNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/HostInteropNode.cpp
IfNode.o: ops/IfNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/IfNode.cpp
ImportNode.o: ops/ImportNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/ImportNode.cpp
InstanceCallNode.o: ops/InstanceCallNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/InstanceCallNode.cpp
InstanceFieldNode.o: ops/InstanceFieldNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/InstanceFieldNode.cpp
InvokeNode.o: ops/InvokeNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/InvokeNode.cpp
IsInstanceNode.o: ops/IsInstanceNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/IsInstanceNode.cpp
KeywordInvokeNode.o: ops/KeywordInvokeNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/KeywordInvokeNode.cpp
LetNode.o: ops/LetNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/LetNode.cpp
LetfnNode.o: ops/LetfnNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/LetfnNode.cpp
LocalNode.o: ops/LocalNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/LocalNode.cpp
LoopNode.o: ops/LoopNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/LoopNode.cpp
MapNode.o: ops/MapNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/MapNode.cpp
MethodNode.o: ops/MethodNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/MethodNode.cpp
MonitorEnterNode.o: ops/MonitorEnterNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/MonitorEnterNode.cpp
MonitorExitNode.o: ops/MonitorExitNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/MonitorExitNode.cpp
MutateSetNode.o: ops/MutateSetNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/MutateSetNode.cpp
NewNode.o: ops/NewNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/NewNode.cpp
PrimInvokeNode.o: ops/PrimInvokeNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/PrimInvokeNode.cpp
ProtocolInvokeNode.o: ops/ProtocolInvokeNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/ProtocolInvokeNode.cpp
QuoteNode.o: ops/QuoteNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/QuoteNode.cpp
RecurNode.o: ops/RecurNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/RecurNode.cpp
ReifyNode.o: ops/ReifyNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/ReifyNode.cpp
SetNode.o: ops/SetNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/SetNode.cpp
StaticCallNode.o: ops/StaticCallNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/StaticCallNode.cpp
StaticFieldNode.o: ops/StaticFieldNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/StaticFieldNode.cpp
TheVarNode.o: ops/TheVarNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/TheVarNode.cpp
ThrowNode.o: ops/ThrowNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/ThrowNode.cpp
TryNode.o: ops/TryNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/TryNode.cpp
VarNode.o: ops/VarNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/VarNode.cpp
VectorNode.o: ops/VectorNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/VectorNode.cpp
WithMetaNode.o: ops/WithMetaNode.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ops/WithMetaNode.cpp
Numbers.o: static/Numbers.cpp static/Numbers.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) static/Numbers.cpp
Utils.o: static/Utils.cpp static/Utils.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) static/Utils.cpp
RuntimeInterop.o: RuntimeInterop.cpp codegen.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) RuntimeInterop.cpp
ObjectTypeSet.o: ObjectTypeSet.cpp ObjectTypeSet.h
	$(CCP) -c $(CFLAGS) $(LLVM_COMPILE_FLAGS) ObjectTypeSet.cpp

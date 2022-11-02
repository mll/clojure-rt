//===- KaleidoscopeJIT.h - A simple JIT for Kaleidoscope --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Contains a simple JIT definition for use in the kaleidoscope tutorials.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EXECUTIONENGINE_ORC_CLOJUREJIT_H
#define LLVM_EXECUTIONENGINE_ORC_CLOJUREJIT_H

#include "llvm/ADT/StringRef.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/ExecutorProcessControl.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"
#include <memory>

namespace llvm {
  namespace orc {  
    class ClojureJIT {
    private:
      std::unique_ptr<ExecutionSession> ES;
      
      DataLayout DL;
      MangleAndInterner Mangle;
      
      RTDyldObjectLinkingLayer ObjectLayer;
      IRCompileLayer CompileLayer;
      
      JITDylib &MainJD;
      
    public:
      ClojureJIT(std::unique_ptr<ExecutionSession> ES, JITTargetMachineBuilder JTMB, DataLayout DL);
      ~ClojureJIT();
      static Expected<std::unique_ptr<ClojureJIT>> Create();
      const DataLayout &getDataLayout() const;
      JITDylib &getMainJITDylib();
      
      Error addModule(ThreadSafeModule TSM, ResourceTrackerSP RT = nullptr);
      Expected<JITEvaluatedSymbol> lookup(StringRef Name);
    };
    
  } // end namespace orc
} // end namespace llvm

#endif // LLVM_EXECUTIONENGINE_ORC_CLOJUREJIT_H

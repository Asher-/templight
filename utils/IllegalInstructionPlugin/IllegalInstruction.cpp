//===- IllegalInstruction.cpp ----------------------*- C++ -*--------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// A plugin for the lit tests, loaded with -load: the pragma
// `#pragma templight_illegal_instruction` executes an illegal instruction, so
// the compilation ends in SIGILL where the pragma stands.
//
//===----------------------------------------------------------------------===//

#include "clang/Lex/Pragma.h"
#include "clang/Lex/Preprocessor.h"

using namespace clang;

namespace {

class IllegalInstructionHandler : public PragmaHandler {
public:
  IllegalInstructionHandler()
      : PragmaHandler("templight_illegal_instruction") {}

  void HandlePragma(Preprocessor &, PragmaIntroducer, Token &) override {
#if defined(__aarch64__)
    asm volatile("udf #0");
#else
    asm volatile("ud2");
#endif
  }
};

} // namespace

static PragmaHandlerRegistry::Add<IllegalInstructionHandler>
    X("templight_illegal_instruction", "executes an illegal instruction");

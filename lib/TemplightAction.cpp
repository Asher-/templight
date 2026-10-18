//===- TemplightAction.cpp --------------------------------*- C++-*--------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "TemplightAction.h"
#include "TemplightDebugger.h"
#include "TemplightTracer.h"

#include "clang/Basic/FileManager.h"
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/MultiplexConsumer.h>
#include <clang/Sema/Sema.h>
#include <clang/Sema/TemplateInstCallback.h>
#include <clang/Serialization/ASTDeserializationListener.h>
#include <clang/Serialization/ASTReader.h>
#include <clang/Serialization/ModuleFile.h>

#include <llvm/ADT/Twine.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/ProgramStack.h>
#include <llvm/Support/Regex.h>

#include <algorithm>

namespace clang {

// Reports each declaration the ASTReader reads to the tracer, with the module
// of the module file it came from and the stack depth at the read.
class TemplightDeserializationListener : public ASTDeserializationListener {
public:
  // Starts reporting to \p T, measuring stack depth below \p Base, or stops
  // when \p T is null.
  void connect(TemplightTracer *T, uintptr_t Base) {
    Tracer = T;
    StackBase = Base;
  }

  void ReaderInitialized(ASTReader *R) override { Reader = R; }

  void DeclRead(GlobalDeclID, const Decl *D) override {
    if (!Tracer || !Reader || !D)
      return;
    serialization::ModuleFile *MF = Reader->getOwningModuleFile(D);
    if (!MF)
      return;
    uintptr_t SP = llvm::getStackPointer();
    // A read on another stack (runWithSufficientStackSpace's) has no
    // depth below the base.
    std::uint64_t DepthKB = StackBase > SP ? (StackBase - SP) / 1024 : 0;
    Tracer->declRead(D, MF->ModuleName, DepthKB);
  }

private:
  ASTReader *Reader = nullptr;
  TemplightTracer *Tracer = nullptr;
  uintptr_t StackBase = 0;
};

namespace {
// Hands the deserialization listener to the ASTReader.
class DeserializationListenerConsumer : public ASTConsumer {
public:
  explicit DeserializationListenerConsumer(TemplightDeserializationListener &L)
      : Listener(L) {}
  ASTDeserializationListener *GetASTDeserializationListener() override {
    return &Listener;
  }

private:
  TemplightDeserializationListener &Listener;
};
} // namespace

std::unique_ptr<clang::ASTConsumer>
TemplightAction::CreateASTConsumer(CompilerInstance &CI, StringRef InFile) {
  std::unique_ptr<ASTConsumer> Consumer =
      WrapperFrontendAction::CreateASTConsumer(CI, InFile);
  if (!Consumer || !InstProfiler || !TraceDeserialization)
    return Consumer;
  DeserializationListener =
      std::make_unique<TemplightDeserializationListener>();
  std::vector<std::unique_ptr<ASTConsumer>> Consumers;
  Consumers.push_back(std::move(Consumer));
  Consumers.push_back(std::make_unique<DeserializationListenerConsumer>(
      *DeserializationListener));
  return std::make_unique<MultiplexConsumer>(std::move(Consumers));
}
bool TemplightAction::BeginInvocation(CompilerInstance &CI) {
  return WrapperFrontendAction::BeginInvocation(CI);
}
bool TemplightAction::BeginSourceFileAction(CompilerInstance &CI) {
  return WrapperFrontendAction::BeginSourceFileAction(CI);
}

std::string TemplightAction::CreateOutputFilename(
    CompilerInstance *CI, const std::string &OptOutputName,
    bool OptInstProfiler, bool OptOutputToStdOut, bool OptMemoryProfile,
    const std::string &OptFormat) {
  std::string result;

  if (!OptInstProfiler) {
    return result; // no need for an output-filename.
  }

  if (OptOutputToStdOut) {
    return "-";
  }
  if (CI && OptOutputName.empty()) {
    result = CI->getFrontendOpts().OutputFile;
  } else {
    result = OptOutputName;
  }

  // Should never get executed.
  if (CI && result.empty()) {
    // then, derive output name from the input name:
    if (CI->hasSourceManager()) {
      FileID fileID = CI->getSourceManager().getMainFileID();
      OptionalFileEntryRef file_ref =
          CI->getSourceManager().getFileEntryRefForID(fileID);
      if (file_ref.has_value()) {
        result = file_ref->getName().str();
      } else {// or, last resort:
        result = "a";
      }
    } else { // or, last resort:
      result = "a";
    }
  }

  if (result.rfind(".trace.") == std::string::npos) {
    result += (OptMemoryProfile ? ".memory.trace." : ".trace.");
    result += TemplightTracer::formatExtension(OptFormat);
  }

  return result;
}

void TemplightAction::EnsureHasSema(CompilerInstance &CI) {
  if (!CI.hasSema()) {
    // This part is normally done by ASTFrontEndAction, but needs to happen
    //  before Templight observers can be created ----------------------->>
    // FIXME: Move the truncation aspect of this into Sema, we delayed this till
    // here so the source manager would be initialized.
    if (hasCodeCompletionSupport() &&
        !CI.getFrontendOpts().CodeCompletionAt.FileName.empty())
      CI.createCodeCompletionConsumer();

    // Use a code completion consumer?
    CodeCompleteConsumer *CompletionConsumer = nullptr;
    if (CI.hasCodeCompletionConsumer())
      CompletionConsumer = &CI.getCodeCompletionConsumer();

    CI.createSema(getTranslationUnitKind(), CompletionConsumer);
    //<<--------------------------------------------------------------
  }
}

void TemplightAction::ExecuteAction() {

  CompilerInstance &CI = WrapperFrontendAction::getCompilerInstance();
  if (!CI.hasPreprocessor())
    return;

  if (InstProfiler) {
    EnsureHasSema(CI);

    std::unique_ptr<TemplightTracer> p_t(
        new TemplightTracer(CI.getSema(), OutputFilename, MemoryProfile,
                            OutputInSafeMode, IgnoreSystemInst, OutputFormat));
    p_t->readBlacklists(BlackListFilename);
    if (DeserializationListener)
      DeserializationListener->connect(p_t.get(), llvm::getStackPointer());
    CI.getSema().TemplateInstCallbacks.push_back(std::move(p_t));
  }
  if (InteractiveDebug) {
    EnsureHasSema(CI);

    std::unique_ptr<TemplightDebugger> p_t(
        new TemplightDebugger(CI.getSema(), MemoryProfile, IgnoreSystemInst));
    p_t->readBlacklists(BlackListFilename);
    CI.getSema().TemplateInstCallbacks.push_back(std::move(p_t));
  }

  WrapperFrontendAction::ExecuteAction();
}
void TemplightAction::EndSourceFileAction() {
  if (DeserializationListener)
    DeserializationListener->connect(nullptr, 0);
  WrapperFrontendAction::EndSourceFileAction();
}

bool TemplightAction::usesPreprocessorOnly() const {
  return WrapperFrontendAction::usesPreprocessorOnly();
}
TranslationUnitKind TemplightAction::getTranslationUnitKind() {
  return WrapperFrontendAction::getTranslationUnitKind();
}
bool TemplightAction::hasPCHSupport() const {
  return WrapperFrontendAction::hasPCHSupport();
}
bool TemplightAction::hasASTFileSupport() const {
  return WrapperFrontendAction::hasASTFileSupport();
}
bool TemplightAction::hasIRSupport() const {
  return WrapperFrontendAction::hasIRSupport();
}
bool TemplightAction::hasCodeCompletionSupport() const {
  return WrapperFrontendAction::hasCodeCompletionSupport();
}

TemplightAction::TemplightAction(std::unique_ptr<FrontendAction> WrappedAction)
    : WrapperFrontendAction(std::move(WrappedAction)), InstProfiler(false),
      OutputToStdOut(false), MemoryProfile(false), OutputInSafeMode(false),
      IgnoreSystemInst(false), InteractiveDebug(false),
      TraceDeserialization(false) {}

TemplightAction::~TemplightAction() = default;

} // namespace clang

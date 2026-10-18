//===- PrintableTemplightEntries.h -------------------*- C++ -*------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_PRINTABLE_TEMPLIGHT_ENTRIES_H
#define LLVM_CLANG_PRINTABLE_TEMPLIGHT_ENTRIES_H

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace llvm {
class raw_ostream;
}

namespace clang {

struct PrintableTemplightEntryBegin {
  int SynthesisKind;
  std::string Name;
  std::string FileName;
  int Line;
  int Column;
  double TimeStamp;
  std::uint64_t MemoryUsage;
  std::string TempOri_FileName;
  int TempOri_Line;
  int TempOri_Column;
};

struct PrintableTemplightEntryEnd {
  double TimeStamp;
  std::uint64_t MemoryUsage;
};

/// -deserialization: a declaration the ASTReader read from a module file.
struct PrintableTemplightEntryDeclRead {
  std::string Kind;
  std::string Name;
  std::string ModuleName;
  std::uint64_t StackDepthKB;
};

/// -deserialization, at the end of the translation unit: a canonical
/// declaration read from two or more module files, and those files' modules.
struct PrintableTemplightEntryMergedDecl {
  std::string Kind;
  std::string Name;
  std::vector<std::string> ModuleNames;
};

class TemplightWriter {
public:
  TemplightWriter(llvm::raw_ostream &aOS) : OutputOS(aOS){};
  virtual ~TemplightWriter(){};

  virtual void initialize(const std::string &aSourceName = "") = 0;
  virtual void finalize() = 0;

  virtual void printEntry(const PrintableTemplightEntryBegin &aEntry) = 0;
  virtual void printEntry(const PrintableTemplightEntryEnd &aEntry) = 0;

  /// -deserialization entries. A format that does not record them writes
  /// nothing.
  virtual void printDeclRead(const PrintableTemplightEntryDeclRead &) {}
  virtual void printMergedDecl(const PrintableTemplightEntryMergedDecl &) {}

  /// Writes the text that closes the trace written so far. Called from the
  /// signal handler of a crashing compiler, so it only writes a buffer that
  /// setCrashClosing() prepared in advance.
  virtual void writeCrashClosing();

protected:
  /// Sets the text writeCrashClosing() writes: what the trace needs after
  /// the entries written so far to be complete in its format.
  void setCrashClosing(std::string Text);

  llvm::raw_ostream &OutputOS;

private:
  // Two buffers, so that a signal handler never reads the one being rewritten.
  std::string CrashClosing[2];
  std::atomic<int> ActiveCrashClosing{-1};
};

} // namespace clang

#endif

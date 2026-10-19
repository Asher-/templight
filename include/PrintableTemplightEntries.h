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

/// Writes a trace in one format. Each entry is formatted in memory and written
/// with one write, so that a crash leaves every entry whole or absent.
class TemplightWriter {
public:
  TemplightWriter(llvm::raw_ostream &aOS) : OutputOS(aOS){};
  virtual ~TemplightWriter();

  virtual void initialize(const std::string &aSourceName = "") = 0;
  virtual void finalize() = 0;

  virtual void printEntry(const PrintableTemplightEntryBegin &aEntry) = 0;
  virtual void printEntry(const PrintableTemplightEntryEnd &aEntry) = 0;

  /// -deserialization entries. A format that does not record them writes
  /// nothing.
  virtual void printDeclRead(const PrintableTemplightEntryDeclRead &) {}
  virtual void printMergedDecl(const PrintableTemplightEntryMergedDecl &) {}

  /// -deserialization, under -safe-mode: the read of \p Entry's declaration
  /// has begun. Until endDeclRead(), a crash writes it as an unfinished read.
  void beginDeclRead(const PrintableTemplightEntryDeclRead &Entry);
  /// The innermost read beginDeclRead() began is finished.
  void endDeclRead();

  /// Writes what the trace written so far needs to be complete in its format:
  /// the crash text of each element still open, innermost first, then the
  /// trace's own closing. Called from the signal handler of a crashing
  /// compiler, so it only writes text prepared in advance.
  virtual void writeCrashClosing();

protected:
  /// Sets the trace's own closing, which a crash writes after the crash text
  /// of every open element.
  void setCrashClosing(std::string Text);

  /// What a crash writes for an element that opens now and closes before
  /// every element open before it.
  struct CrashText {
    std::string Text;
    const CrashText *Outer = nullptr;
  };
  /// Prepared before the element's opening is written, published once it is
  /// and retired once the element's own closing is written, so that a crash at
  /// any point finds the crash texts that match what the file holds.
  static CrashText *prepareCrashText(std::string Text);
  void publishCrashText(CrashText *Text);
  void retireCrashText();
  /// Writes the crash text of each open element, innermost first, and returns
  /// whether there was any.
  bool writeOpenCrashTexts();

  /// -deserialization, under -safe-mode: what a crash writes for a read still
  /// under way. A format that does not record reads writes nothing.
  virtual std::string
  unfinishedDeclReadText(const PrintableTemplightEntryDeclRead &) {
    return "";
  }

  llvm::raw_ostream &OutputOS;

private:
  // Two buffers, so that a signal handler never reads the one being rewritten.
  std::string CrashClosing[2];
  std::atomic<int> ActiveCrashClosing{-1};
  // The open elements' crash texts, innermost first. A published text does not
  // change until it is retired.
  std::atomic<const CrashText *> InnermostCrashText{nullptr};
};

} // namespace clang

#endif

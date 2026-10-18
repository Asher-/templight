//===- TemplightTracer.h ---------------------------*- C++ -*--------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TEMPLIGHT_TRACER_H
#define LLVM_CLANG_TEMPLIGHT_TRACER_H

#include "clang/Sema/TemplateInstCallback.h"

#include <memory>
#include <string>

namespace clang {

class TemplightTracer : public TemplateInstantiationCallback {
public:
  class TracePrinter; // forward-decl.

  void initialize(const Sema &TheSema) override;
  void finalize(const Sema &TheSema) override;
  void atTemplateBegin(const Sema &TheSema,
                       const Sema::CodeSynthesisContext &Inst) override;
  void atTemplateEnd(const Sema &TheSema,
                     const Sema::CodeSynthesisContext &Inst) override;

private:
  unsigned MemoryFlag : 1;
  unsigned SafeModeFlag : 1;

  std::unique_ptr<TracePrinter> Printer;

public:
  /// \brief Writes the template trace to Output in Format, one of the names
  /// isFormat() accepts.
  TemplightTracer(const Sema &TheSema, std::string Output = "",
                  bool Memory = false, bool Safemode = false,
                  bool IgnoreSystem = false,
                  const std::string &Format = "protobuf");

  /// Whether Format names a trace format: protobuf, yaml, xml, text,
  /// nestedxml, graphml or graphviz.
  static bool isFormat(const std::string &Format);
  /// The accepted format names, comma-separated, for diagnostics.
  static const char *formatList();
  /// The trace file extension for Format (pbf for protobuf).
  static const char *formatExtension(const std::string &Format);

  ~TemplightTracer() override;

  bool getMemoryFlag() const { return MemoryFlag; };
  bool getSafeModeFlag() const { return SafeModeFlag; };

  void readBlacklists(const std::string &BLFilename);
};

} // namespace clang

#endif

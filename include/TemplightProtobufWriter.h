//===- TemplightProtobufWriter.h --------------------*- C++ -*-------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TEMPLIGHT_PROTOBUF_WRITER_H
#define LLVM_CLANG_TEMPLIGHT_PROTOBUF_WRITER_H

#include "PrintableTemplightEntries.h"

#include "llvm/ADT/StringRef.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace llvm {
class raw_ostream;
}

namespace clang {

class TemplightProtobufWriter : public TemplightWriter {
private:
  std::string buffer;
  std::unordered_map<std::string, std::size_t> fileNameMap;
  std::unordered_map<std::string, std::size_t> templateNameMap;
  int compressionMode;

  /// Streaming (safe-mode) output: every field of the trace reaches the
  /// output stream as soon as it is produced, so a compiler crash leaves a
  /// readable trace of everything recorded before it.
  bool Streaming;
  /// Streaming into a seekable file: the trace is one TemplightTrace record
  /// whose length is a fixed-width varint at LengthOffset, rewritten after
  /// every append. Otherwise each append is a TemplightTrace record of its own.
  bool Seekable = false;
  std::uint64_t LengthOffset = 0;
  std::uint64_t BodySize = 0;

  void appendToTrace(llvm::StringRef Field);
  // Appends a TemplightEntry whose field 'OneofField' holds 'Contents'.
  void appendEntry(unsigned OneofField, const std::string &Contents);
  void patchLength();

  std::size_t createDictionaryEntry(const std::string &Name);
  std::string printEntryLocation(const std::string &FileName, int Line,
                                 int Column);
  std::string printTemplateName(const std::string &Name);

public:
  TemplightProtobufWriter(llvm::raw_ostream &aOS, int aCompressLevel = 2,
                          bool aStreaming = false);

  void initialize(const std::string &aSourceName = "") override;
  void finalize() override;

  void printEntry(const PrintableTemplightEntryBegin &aEntry) override;
  void printEntry(const PrintableTemplightEntryEnd &aEntry) override;
  void printDeclRead(const PrintableTemplightEntryDeclRead &aEntry) override;
  void
  printMergedDecl(const PrintableTemplightEntryMergedDecl &aEntry) override;
};

} // namespace clang

#endif

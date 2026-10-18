//===- unittests/TemplightProtobufReaderTest.cpp --------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "../utils/ProtobufReader/TemplightProtobufReader.h"
#include "PrintableTemplightEntries.h"
#include "TemplightProtobufWriter.h"

#include "llvm/Support/raw_ostream.h"
#include "gtest/gtest.h"

#include <string>
#include <vector>

using namespace clang;

namespace {

PrintableTemplightEntryBegin beginEntry(const char *Name, int Line) {
  PrintableTemplightEntryBegin Entry = {};
  Entry.SynthesisKind = 0;
  Entry.Name = Name;
  Entry.FileName = "trace-source.cpp";
  Entry.Line = Line;
  Entry.Column = 3;
  return Entry;
}

// Writes Square<3> and Square<2>, each begun and ended, to \p OS.
void writeTrace(TemplightProtobufWriter &Writer) {
  Writer.initialize("trace-source.cpp");
  Writer.printEntry(beginEntry("Square<3>", 7));
  Writer.printEntry(PrintableTemplightEntryEnd{});
  Writer.printEntry(beginEntry("Square<2>", 8));
  Writer.printEntry(PrintableTemplightEntryEnd{});
  Writer.finalize();
}

// Reads \p Trace with utils/ProtobufReader, one "name file:line" per begin.
std::vector<std::string> readBegins(const std::string &Trace,
                                    std::string &SourceName) {
  std::vector<std::string> Begins;
  TemplightProtobufReader Reader;
  for (TemplightProtobufReader::LastChunkType Chunk =
           Reader.startOnBuffer(Trace);
       Chunk != TemplightProtobufReader::EndOfFile; Chunk = Reader.next())
    if (Chunk == TemplightProtobufReader::BeginEntry)
      Begins.push_back(Reader.LastBeginEntry.Name + " " +
                       Reader.LastBeginEntry.FileName + ":" +
                       std::to_string(Reader.LastBeginEntry.Line));
  SourceName = Reader.SourceName;
  return Begins;
}

const std::vector<std::string> ExpectedBegins = {
    "Square<3> trace-source.cpp:7", "Square<2> trace-source.cpp:8"};

TEST(TemplightProtobufReader, ReadsABufferedTrace) {
  std::string Trace;
  llvm::raw_string_ostream OS(Trace);
  TemplightProtobufWriter Writer(OS);
  writeTrace(Writer);
  OS.flush();
  std::string SourceName;
  EXPECT_EQ(readBegins(Trace, SourceName), ExpectedBegins);
  EXPECT_EQ(SourceName, "trace-source.cpp");
}

// A -safe-mode trace written to a pipe cannot be patched in place, so each
// entry is a trace record of its own; the reader reads them on as one trace,
// keeping the names and file names the earlier records defined.
TEST(TemplightProtobufReader, ReadsAStreamedPipeTraceAsOneTrace) {
  std::string Trace;
  llvm::raw_string_ostream OS(Trace);
  TemplightProtobufWriter Writer(OS, /*aCompressLevel=*/2, /*aStreaming=*/true);
  writeTrace(Writer);
  OS.flush();
  std::string SourceName;
  EXPECT_EQ(readBegins(Trace, SourceName), ExpectedBegins);
  EXPECT_EQ(SourceName, "trace-source.cpp");
}

} // namespace

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

#include "llvm/ADT/STLFunctionalExtras.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
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

// Writes the -deserialization entries of Chain<3> read from two modules: one
// per read, then the declaration the two reads merge into.
void writeDeserializationEntries(TemplightProtobufWriter &Writer) {
  Writer.printDeclRead(
      {"ClassTemplateSpecialization", "Chain<3>", "DeserA", 26});
  Writer.printDeclRead(
      {"ClassTemplateSpecialization", "Chain<3>", "DeserB", 27});
  Writer.printMergedDecl(
      {"ClassTemplateSpecialization", "Chain<3>", {"DeserA", "DeserB"}});
}

// Reads \p Trace with utils/ProtobufReader, one line per -deserialization
// entry.
std::vector<std::string> readDeserializationEntries(const std::string &Trace) {
  std::vector<std::string> Entries;
  TemplightProtobufReader Reader;
  for (TemplightProtobufReader::LastChunkType Chunk =
           Reader.startOnBuffer(Trace);
       Chunk != TemplightProtobufReader::EndOfFile; Chunk = Reader.next()) {
    if (Chunk == TemplightProtobufReader::DeclReadEntry) {
      Entries.push_back("read " + Reader.LastDeclRead.Kind + " " +
                        Reader.LastDeclRead.Name + " " +
                        Reader.LastDeclRead.ModuleName + " " +
                        std::to_string(Reader.LastDeclRead.StackDepthKB));
    } else if (Chunk == TemplightProtobufReader::MergedDeclEntry) {
      std::string Entry = "merged " + Reader.LastMergedDecl.Kind + " " +
                          Reader.LastMergedDecl.Name;
      for (const std::string &ModuleName : Reader.LastMergedDecl.ModuleNames)
        Entry += " " + ModuleName;
      Entries.push_back(Entry);
    }
  }
  return Entries;
}

const std::vector<std::string> ExpectedDeserializationEntries = {
    "read ClassTemplateSpecialization Chain<3> DeserA 26",
    "read ClassTemplateSpecialization Chain<3> DeserB 27",
    "merged ClassTemplateSpecialization Chain<3> DeserA DeserB"};

// Lets \p Write write a trace into a temporary file, as -output=<file> does,
// and returns what the file then holds.
std::string
writeToFile(llvm::function_ref<void(llvm::raw_fd_ostream &)> Write) {
  llvm::SmallString<128> Path;
  EXPECT_FALSE(llvm::sys::fs::createTemporaryFile("templight", "pbf", Path));
  {
    std::error_code EC;
    llvm::raw_fd_ostream OS(Path, EC);
    EXPECT_FALSE(EC);
    Write(OS);
  }
  auto Buffer = llvm::MemoryBuffer::getFile(Path);
  llvm::sys::fs::remove(Path);
  EXPECT_TRUE(bool(Buffer));
  return Buffer ? (*Buffer)->getBuffer().str() : std::string();
}

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

// A -safe-mode trace written to a file patches its length in place after each
// entry, so the file is one trace that the reader reads whole, and so is the
// file a crash leaves before the trace is finalized.
TEST(TemplightProtobufReader, ReadsAStreamedFileTraceFinishedOrCutShort) {
  for (bool Finish : {true, false}) {
    std::string Trace = writeToFile([Finish](llvm::raw_fd_ostream &OS) {
      TemplightProtobufWriter Writer(OS, /*aCompressLevel=*/2,
                                     /*aStreaming=*/true);
      Writer.initialize("trace-source.cpp");
      Writer.printEntry(beginEntry("Square<3>", 7));
      Writer.printEntry(PrintableTemplightEntryEnd{});
      Writer.printEntry(beginEntry("Square<2>", 8));
      Writer.printEntry(PrintableTemplightEntryEnd{});
      if (Finish)
        Writer.finalize();
    });
    std::string SourceName;
    EXPECT_EQ(readBegins(Trace, SourceName), ExpectedBegins)
        << (Finish ? "finished" : "cut short");
    EXPECT_EQ(SourceName, "trace-source.cpp");
  }
}

// -deserialization entries (decl_read and merged_decl) are read back, names
// included, from a buffered trace, a streamed pipe trace and a streamed file
// trace.
TEST(TemplightProtobufReader, ReadsDeserializationEntriesInEveryLayout) {
  {
    std::string Trace;
    llvm::raw_string_ostream OS(Trace);
    TemplightProtobufWriter Writer(OS);
    Writer.initialize("trace-source.cpp");
    writeDeserializationEntries(Writer);
    Writer.finalize();
    OS.flush();
    EXPECT_EQ(readDeserializationEntries(Trace), ExpectedDeserializationEntries)
        << "buffered";
  }
  {
    std::string Trace;
    llvm::raw_string_ostream OS(Trace);
    TemplightProtobufWriter Writer(OS, /*aCompressLevel=*/2,
                                   /*aStreaming=*/true);
    Writer.initialize("trace-source.cpp");
    writeDeserializationEntries(Writer);
    Writer.finalize();
    OS.flush();
    EXPECT_EQ(readDeserializationEntries(Trace), ExpectedDeserializationEntries)
        << "streamed to a pipe";
  }
  std::string Trace = writeToFile([](llvm::raw_fd_ostream &OS) {
    TemplightProtobufWriter Writer(OS, /*aCompressLevel=*/2,
                                   /*aStreaming=*/true);
    Writer.initialize("trace-source.cpp");
    writeDeserializationEntries(Writer);
    Writer.finalize();
  });
  EXPECT_EQ(readDeserializationEntries(Trace), ExpectedDeserializationEntries)
      << "streamed to a file";
}

} // namespace

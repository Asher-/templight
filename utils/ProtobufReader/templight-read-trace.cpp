//===- templight-read-trace.cpp --------------------*- C++ -*--------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// templight-read-trace < trace.pbf prints a templight protobuf trace, read
// from standard input with TemplightProtobufReader, one line per header and
// entry. The lines are those test/Inputs/read-trace.py prints, so a lit test
// checks the trace through both readers with the same CHECK lines.
//
//===----------------------------------------------------------------------===//

#include "TemplightProtobufReader.h"

#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;

int main() {
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> Input =
      llvm::MemoryBuffer::getSTDIN();
  if (!Input) {
    llvm::errs() << "templight-read-trace: cannot read standard input: "
                 << Input.getError().message() << "\n";
    return 1;
  }

  llvm::raw_ostream &OS = llvm::outs();
  TemplightProtobufReader Reader;
  for (TemplightProtobufReader::LastChunkType Chunk =
           Reader.startOnBuffer((*Input)->getBuffer());
       Chunk != TemplightProtobufReader::EndOfFile; Chunk = Reader.next()) {
    switch (Chunk) {
    case TemplightProtobufReader::Header:
      OS << "header version=" << Reader.Version
         << " source=" << Reader.SourceName << "\n";
      break;
    case TemplightProtobufReader::BeginEntry:
      OS << "begin kind=" << Reader.LastBeginEntry.SynthesisKind
         << " name=" << Reader.LastBeginEntry.Name
         << " at=" << Reader.LastBeginEntry.FileName << ":"
         << Reader.LastBeginEntry.Line << ":" << Reader.LastBeginEntry.Column
         << "\n";
      break;
    case TemplightProtobufReader::EndEntry:
      OS << "end\n";
      break;
    case TemplightProtobufReader::DeclReadEntry:
      OS << "decl-read kind=" << Reader.LastDeclRead.Kind
         << " name=" << Reader.LastDeclRead.Name
         << " module=" << Reader.LastDeclRead.ModuleName
         << " stack-kb=" << Reader.LastDeclRead.StackDepthKB << "\n";
      break;
    case TemplightProtobufReader::MergedDeclEntry:
      OS << "merged-decl kind=" << Reader.LastMergedDecl.Kind
         << " name=" << Reader.LastMergedDecl.Name
         << " modules=" << llvm::join(Reader.LastMergedDecl.ModuleNames, ",")
         << "\n";
      break;
    default:
      break;
    }
  }
  OS << "entries-done\n";
  return 0;
}

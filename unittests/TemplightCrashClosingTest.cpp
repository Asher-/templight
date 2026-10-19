//===- unittests/TemplightCrashClosingTest.cpp ----------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// A -safe-mode trace that a crash cuts at any of its writes, closed as the
// crash handler closes it, is complete in its format and holds every entry
// written before the cut, then the declaration read the cut left unfinished.
//
//===----------------------------------------------------------------------===//

#include "../utils/ExtraWriters/TemplightExtraWriters.h"
#include "../utils/ProtobufReader/TemplightProtobufReader.h"
#include "PrintableTemplightEntries.h"
#include "TemplightProtobufWriter.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/YAMLParser.h"
#include "llvm/Support/raw_ostream.h"
#include "gtest/gtest.h"

#ifdef LLVM_ON_UNIX
#include <unistd.h>
#endif

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace clang;

namespace {

struct TraceCut {};

// A trace stream that a crash cuts at its N-th write once armed (the tracer
// arms the crash closing once the writer is made): that write throws before
// any of it is written, as a signal at that point leaves the file. What the
// crash handler then writes goes through, and nothing after it.
class CuttingStream : public llvm::raw_ostream {
public:
  CuttingStream(std::string &Out, unsigned WritesBeforeCut)
      : llvm::raw_ostream(/*unbuffered=*/true), Out(Out),
        WritesBeforeCut(WritesBeforeCut) {}
  ~CuttingStream() override { flush(); }

  void arm() { State = Writing; }
  // finalize() ends what the crash closing covers.
  void disarm() { State = Unarmed; }
  bool isCut() const { return State == Closing || State == Discarding; }
  void endCrash() { State = Discarding; }

private:
  enum { Unarmed, Writing, Closing, Discarding } State = Unarmed;
  std::string &Out;
  unsigned WritesBeforeCut;

  void write_impl(const char *Ptr, size_t Size) override {
    if (State == Writing && WritesBeforeCut-- == 0) {
      State = Closing;
      throw TraceCut();
    }
    if (State != Discarding)
      Out.append(Ptr, Size);
  }
  uint64_t current_pos() const override { return Out.size(); }
};

PrintableTemplightEntryBegin beginEntry(const char *Name) {
  PrintableTemplightEntryBegin Entry = {};
  Entry.Name = Name;
  Entry.FileName = "cut.cpp";
  Entry.Line = 3;
  Entry.Column = 5;
  return Entry;
}

PrintableTemplightEntryDeclRead declRead(const char *Name) {
  return {"CXXRecord", Name, "CutModule", 12};
}

// Instantiations A, B within A and C within A; reads X and Y within B, and Z
// within Y's read; then M, read from two modules.
void writeTrace(TemplightWriter &Writer) {
  Writer.initialize("cut.cpp");
  Writer.printEntry(beginEntry("A"));
  Writer.printEntry(beginEntry("B"));
  Writer.printDeclRead(declRead("X"));
  Writer.beginDeclRead(declRead("Y"));
  Writer.printDeclRead(declRead("Z"));
  Writer.printDeclRead(declRead("Y"));
  Writer.endDeclRead();
  Writer.printEntry(PrintableTemplightEntryEnd{});
  Writer.printEntry(beginEntry("C"));
  Writer.printEntry(PrintableTemplightEntryEnd{});
  Writer.printEntry(PrintableTemplightEntryEnd{});
  Writer.printMergedDecl({"CXXRecord", "M", {"CutModule", "OtherModule"}});
  Writer.finalize();
}

// What every format reads back from the whole trace.
const std::vector<std::string> WholeTrace = {
    "begin A", "begin B", "read X", "read Z", "read Y",
    "end",     "begin C", "end",    "end",    "merged M"};

using Entries = std::optional<std::vector<std::string>>;

//===- XML ----------------------------------------------------------------===//

struct XmlElement {
  std::string Name;
  std::map<std::string, std::string> Attributes;
  std::string Text;
  std::vector<XmlElement> Children;
};

// Parses a document of one element, refusing one that is not well-formed.
class XmlParser {
  llvm::StringRef S;

  bool skipMisc() {
    for (;;) {
      S = S.ltrim();
      llvm::StringRef End;
      if (S.starts_with("<?"))
        End = "?>";
      else if (S.starts_with("<!--"))
        End = "-->";
      else
        return true;
      size_t At = S.find(End);
      if (At == llvm::StringRef::npos)
        return false;
      S = S.drop_front(At + End.size());
    }
  }

  bool name(std::string &Name) {
    size_t I = 0;
    while (I < S.size() &&
           (llvm::isAlnum(S[I]) || llvm::is_contained("_:-.", S[I])))
      ++I;
    if (!I)
      return false;
    Name = S.take_front(I).str();
    S = S.drop_front(I);
    return true;
  }

  bool element(XmlElement &E) {
    if (!S.consume_front("<") || !name(E.Name))
      return false;
    for (;;) {
      S = S.ltrim();
      if (S.consume_front("/>"))
        return true;
      if (S.consume_front(">"))
        break;
      std::string Attribute;
      if (!name(Attribute))
        return false;
      S = S.ltrim();
      if (!S.consume_front("="))
        return false;
      S = S.ltrim();
      if (!S.consume_front("\""))
        return false;
      size_t Quote = S.find('"');
      if (Quote == llvm::StringRef::npos || S.take_front(Quote).contains('<'))
        return false;
      E.Attributes[Attribute] = S.take_front(Quote).str();
      S = S.drop_front(Quote + 1);
    }
    for (;;) {
      size_t Open = S.find('<');
      if (Open == llvm::StringRef::npos)
        return false;
      E.Text += S.take_front(Open).str();
      S = S.drop_front(Open);
      if (S.consume_front("</")) {
        std::string Name;
        if (!name(Name) || Name != E.Name)
          return false;
        S = S.ltrim();
        return S.consume_front(">");
      }
      E.Children.emplace_back();
      if (!element(E.Children.back()))
        return false;
    }
  }

public:
  bool document(llvm::StringRef Document, XmlElement &Root) {
    S = Document;
    return skipMisc() && element(Root) && skipMisc() && S.empty();
  }
};

std::string childText(const XmlElement &E, llvm::StringRef Child) {
  for (const XmlElement &C : E.Children)
    if (C.Name == Child)
      return C.Text;
  return "?";
}

std::optional<XmlElement> parseXml(const std::string &Trace) {
  XmlElement Root;
  if (!XmlParser().document(Trace, Root))
    return std::nullopt;
  return Root;
}

Entries readXml(const std::string &Trace) {
  std::optional<XmlElement> Root = parseXml(Trace);
  if (!Root || Root->Name != "Trace")
    return std::nullopt;
  std::vector<std::string> Got;
  for (const XmlElement &E : Root->Children) {
    if (E.Name == "TemplateBegin") {
      for (const XmlElement &C : E.Children)
        if (C.Name == "Context")
          Got.push_back("begin " + C.Attributes.at("context"));
    } else if (E.Name == "TemplateEnd")
      Got.push_back("end");
    else if (E.Name == "DeclRead")
      Got.push_back("read " + childText(E, "Name"));
    else if (E.Name == "UnfinishedDeclRead")
      Got.push_back("unfinished " + childText(E, "Name"));
    else if (E.Name == "MergedDecl")
      Got.push_back("merged " + childText(E, "Name"));
  }
  return Got;
}

void readNestedEntries(const XmlElement &E, std::vector<std::string> &Got) {
  for (const XmlElement &C : E.Children) {
    if (C.Name == "Entry") {
      Got.push_back("begin " + C.Attributes.at("Name"));
      readNestedEntries(C, Got);
    } else if (C.Name == "Exit")
      Got.push_back("end");
    else if (C.Name == "DeclRead")
      Got.push_back("read " + C.Attributes.at("Name"));
    else if (C.Name == "UnfinishedDeclRead")
      Got.push_back("unfinished " + C.Attributes.at("Name"));
    else if (C.Name == "MergedDecl")
      Got.push_back("merged " + C.Attributes.at("Name"));
  }
}

Entries readNestedXml(const std::string &Trace) {
  std::optional<XmlElement> Root = parseXml(Trace);
  if (!Root || Root->Name != "Trace")
    return std::nullopt;
  std::vector<std::string> Got;
  readNestedEntries(*Root, Got);
  return Got;
}

Entries readGraphML(const std::string &Trace) {
  std::optional<XmlElement> Root = parseXml(Trace);
  if (!Root || Root->Name != "graphml")
    return std::nullopt;
  std::vector<std::string> Got;
  for (const XmlElement &Graph : Root->Children) {
    if (Graph.Name != "graph")
      continue;
    for (const XmlElement &E : Graph.Children) {
      std::string ID = E.Attributes.count("id") ? E.Attributes.at("id") : "";
      if (E.Name == "edge") {
        // A crash closes an open entry with an edge that has no measures.
        if (!E.Children.empty())
          Got.push_back("end");
        continue;
      }
      std::string Name;
      for (const XmlElement &Data : E.Children)
        if (Data.Attributes.count("key") && Data.Attributes.at("key") == "d1")
          Name = llvm::StringRef(Data.Text).trim('"').str();
      if (ID[0] == 'n')
        Got.push_back("begin " + Name);
      else if (ID[0] == 'r')
        Got.push_back("read " + Name);
      else if (ID[0] == 'u')
        Got.push_back("unfinished " + Name);
      else if (ID[0] == 'm')
        Got.push_back("merged " + Name);
    }
  }
  return Got;
}

//===- GraphViz, YAML and text
//---------------------------------------------===//

// The parts of a DOT label, which separates them with the two characters \n.
std::vector<std::string> labelParts(llvm::StringRef Line) {
  size_t Open = Line.find('"');
  llvm::StringRef Label = Line.drop_front(Open + 1);
  Label = Label.take_front(Label.find('"'));
  llvm::SmallVector<llvm::StringRef, 6> Parts;
  Label.split(Parts, "\\n");
  return std::vector<std::string>(Parts.begin(), Parts.end());
}

Entries readGraphViz(const std::string &Trace) {
  llvm::StringRef S(Trace);
  if (!S.consume_front("digraph Trace {\n") || !S.consume_back("}\n"))
    return std::nullopt;
  std::vector<std::string> Got;
  std::map<std::string, bool> Begun;
  llvm::SmallVector<llvm::StringRef, 32> Lines;
  S.split(Lines, '\n', -1, /*KeepEmpty=*/false);
  for (llvm::StringRef Line : Lines) {
    // A statement written whole ends its line.
    if (!Line.ends_with(";") || llvm::count(Line, '"') % 2)
      return std::nullopt;
    if (Line.contains("->"))
      continue;
    std::string ID = Line.take_until([](char C) { return C == ' '; }).str();
    std::vector<std::string> Parts = labelParts(Line);
    if (ID[0] == 'n' && Parts.size() > 1)
      Got.push_back(Begun[ID] ? "end" : "begin " + Parts[1]);
    else if (ID[0] == 'd' && Parts.size() > 2)
      Got.push_back((Parts[0] == "DeclRead" ? "read " : "merged ") + Parts[2]);
    else if (ID[0] == 'u' && Parts.size() > 2)
      Got.push_back("unfinished " + Parts[2]);
    Begun[ID] = true;
  }
  return Got;
}

Entries readYaml(const std::string &Trace) {
  llvm::SourceMgr SM;
  SM.setDiagHandler([](const llvm::SMDiagnostic &, void *) {});
  llvm::yaml::Stream Stream(Trace, SM);
  std::vector<std::string> Got;
  unsigned Documents = 0;
  for (llvm::yaml::Document &Document : Stream) {
    ++Documents;
    auto *Sequence =
        llvm::dyn_cast_or_null<llvm::yaml::SequenceNode>(Document.getRoot());
    if (!Sequence)
      return std::nullopt;
    for (llvm::yaml::Node &Item : *Sequence) {
      auto *Mapping = llvm::dyn_cast<llvm::yaml::MappingNode>(&Item);
      if (!Mapping)
        return std::nullopt;
      std::string First, FirstValue, Name;
      for (llvm::yaml::KeyValueNode &Pair : *Mapping) {
        auto *Key =
            llvm::dyn_cast_or_null<llvm::yaml::ScalarNode>(Pair.getKey());
        auto *Value =
            llvm::dyn_cast_or_null<llvm::yaml::ScalarNode>(Pair.getValue());
        if (!Key)
          return std::nullopt;
        llvm::SmallString<32> KeyStorage, ValueStorage;
        std::string K = Key->getValue(KeyStorage).str();
        std::string V = Value ? Value->getValue(ValueStorage).str() : "";
        if (First.empty()) {
          First = K;
          FirstValue = V;
        }
        if (K == "Name")
          Name = V;
      }
      if (First == "IsBegin")
        Got.push_back(FirstValue == "true" ? "begin " + Name : "end");
      else if (First == "DeclRead")
        Got.push_back("read " + Name);
      else if (First == "UnfinishedDeclRead")
        Got.push_back("unfinished " + Name);
      else if (First == "MergedDecl")
        Got.push_back("merged " + Name);
    }
  }
  if (Stream.failed() || Documents != 1)
    return std::nullopt;
  return Got;
}

Entries readText(const std::string &Trace) {
  // The fields each entry has, so that a cut entry is told from a whole one.
  static const std::map<std::string, std::vector<std::string>> Fields = {
      {"TemplateBegin",
       {"Kind", "Name", "Location", "TimeStamp", "MemoryUsage"}},
      {"TemplateEnd", {"TimeStamp", "MemoryUsage"}},
      {"DeclRead", {"Kind", "Name", "Module", "StackDepthKB"}},
      {"UnfinishedDeclRead", {"Kind", "Name", "Module", "StackDepthKB"}},
      {"MergedDecl", {"Kind", "Name", "Module"}}};
  std::vector<std::pair<std::string, std::map<std::string, std::string>>>
      Blocks;
  llvm::SmallVector<llvm::StringRef, 64> Lines;
  llvm::StringRef(Trace).split(Lines, '\n', -1, /*KeepEmpty=*/false);
  for (llvm::StringRef Line : Lines) {
    if (Line.starts_with("  SourceFile = "))
      continue;
    if (!Line.starts_with(" ")) {
      if (!Fields.count(Line.str()))
        return std::nullopt;
      Blocks.push_back({Line.str(), {}});
      continue;
    }
    auto [Key, Value] = Line.trim().split(" = ");
    if (Blocks.empty())
      return std::nullopt;
    Blocks.back().second[Key.str()] = Value.str();
  }
  std::vector<std::string> Got;
  for (auto &[Entry, Values] : Blocks) {
    for (const std::string &Field : Fields.at(Entry))
      if (!Values.count(Field))
        return std::nullopt;
    std::string Name = Values["Name"];
    if (Entry == "TemplateBegin")
      Got.push_back("begin " + Name);
    else if (Entry == "TemplateEnd")
      Got.push_back("end");
    else if (Entry == "DeclRead")
      Got.push_back("read " + Name);
    else if (Entry == "UnfinishedDeclRead")
      Got.push_back("unfinished " + Name);
    else
      Got.push_back("merged " + Name);
  }
  return Got;
}

//===- Protobuf
//------------------------------------------------------------===//

// Whether \p Trace is whole TemplightTrace records and nothing else.
bool isWholeRecords(llvm::StringRef Trace) {
  while (!Trace.empty()) {
    if (Trace.front() != 0x0A) // traces = 1, length-delimited
      return false;
    Trace = Trace.drop_front();
    uint64_t Size = 0;
    for (unsigned Shift = 0;; Shift += 7) {
      if (Trace.empty())
        return false;
      uint8_t Byte = Trace.front();
      Trace = Trace.drop_front();
      Size |= uint64_t(Byte & 0x7F) << Shift;
      if (!(Byte & 0x80))
        break;
    }
    if (Size > Trace.size())
      return false;
    Trace = Trace.drop_front(Size);
  }
  return true;
}

Entries readProtobuf(const std::string &Trace) {
  if (!isWholeRecords(Trace))
    return std::nullopt;
  std::vector<std::string> Got;
  TemplightProtobufReader Reader;
  for (TemplightProtobufReader::LastChunkType Chunk =
           Reader.startOnBuffer(Trace);
       Chunk != TemplightProtobufReader::EndOfFile; Chunk = Reader.next()) {
    if (Chunk == TemplightProtobufReader::BeginEntry)
      Got.push_back("begin " + Reader.LastBeginEntry.Name);
    else if (Chunk == TemplightProtobufReader::EndEntry)
      Got.push_back("end");
    else if (Chunk == TemplightProtobufReader::DeclReadEntry)
      Got.push_back("read " + Reader.LastDeclRead.Name);
    else if (Chunk == TemplightProtobufReader::UnfinishedDeclReadEntry)
      Got.push_back("unfinished " + Reader.LastDeclRead.Name);
    else if (Chunk == TemplightProtobufReader::MergedDeclEntry)
      Got.push_back("merged " + Reader.LastMergedDecl.Name);
  }
  return Got;
}

//===- The cut
//-------------------------------------------------------------===//

struct Format {
  const char *Name;
  std::function<TemplightWriter *(llvm::raw_ostream &)> Create;
  std::function<Entries(const std::string &)> Read;
};

const Format Formats[] = {
    {"yaml", [](llvm::raw_ostream &OS) { return new TemplightYamlWriter(OS); },
     readYaml},
    {"xml", [](llvm::raw_ostream &OS) { return new TemplightXmlWriter(OS); },
     readXml},
    {"text", [](llvm::raw_ostream &OS) { return new TemplightTextWriter(OS); },
     readText},
    {"nestedxml",
     [](llvm::raw_ostream &OS) {
       return new TemplightNestedXMLWriter(OS, /*aStreaming=*/true);
     },
     readNestedXml},
    {"graphml",
     [](llvm::raw_ostream &OS) {
       return new TemplightGraphMLWriter(OS, /*aStreaming=*/true);
     },
     readGraphML},
    {"graphviz",
     [](llvm::raw_ostream &OS) {
       return new TemplightGraphVizWriter(OS, /*aStreaming=*/true);
     },
     readGraphViz},
    {"protobuf",
     [](llvm::raw_ostream &OS) {
       return new TemplightProtobufWriter(OS, /*aCompressLevel=*/2,
                                          /*aStreaming=*/true);
     },
     readProtobuf}};

// \p Got, read from a trace cut and closed, is the whole trace's first
// entries, then Y's read when the cut left it under way: after Z's read and
// before Y's, or at the write that would have written Z's.
void expectCutTrace(const std::vector<std::string> &Got,
                    const std::string &Where) {
  size_t Written = Got.size();
  bool Unfinished = Written && Got.back() == "unfinished Y";
  if (Unfinished)
    --Written;
  ASSERT_LE(Written, WholeTrace.size()) << Where;
  EXPECT_EQ(std::vector<std::string>(Got.begin(), Got.begin() + Written),
            std::vector<std::string>(WholeTrace.begin(),
                                     WholeTrace.begin() + Written))
      << Where;
  std::string Last = Written ? WholeTrace[Written - 1] : "";
  if (Last == "read Z")
    EXPECT_TRUE(Unfinished) << Where;
  else if (Last != "read X")
    EXPECT_FALSE(Unfinished) << Where;
}

TEST(TemplightCrashClosing, CutsATraceAtEachWriteInEveryFormat) {
  for (const Format &F : Formats) {
    for (unsigned Cut = 0;; ++Cut) {
      std::string Trace;
      CuttingStream OS(Trace, Cut);
      std::unique_ptr<TemplightWriter> Writer(F.Create(OS));
      OS.arm();
      try {
        writeTrace(*Writer);
      } catch (TraceCut &) {
      }
      std::string Where =
          std::string(F.Name) + " cut at write " + std::to_string(Cut);
      if (!OS.isCut()) {
        OS.disarm();
        Writer.reset();
        Entries Got = F.Read(Trace);
        ASSERT_TRUE(Got.has_value()) << F.Name << " whole:\n" << Trace;
        EXPECT_EQ(*Got, WholeTrace) << F.Name << " whole";
        break;
      }
      Writer->writeCrashClosing();
      OS.endCrash();
      Writer.reset();
      Entries Got = F.Read(Trace);
      ASSERT_TRUE(Got.has_value()) << Where << ":\n" << Trace;
      expectCutTrace(*Got, Where);
    }
  }
}

#ifdef LLVM_ON_UNIX
// A -safe-mode protobuf trace in a file records its length after each field,
// so a crash can come between a field and its length. The crash closing sets
// the length to cover every field in the file, and the read under way.
TEST(TemplightCrashClosing, CoversAFieldWrittenBeforeItsLength) {
  llvm::SmallString<128> Path;
  ASSERT_FALSE(llvm::sys::fs::createTemporaryFile("templight", "pbf", Path));
  {
    int FD;
    ASSERT_FALSE(llvm::sys::fs::openFileForWrite(Path, FD));
    llvm::raw_fd_ostream OS(FD, /*shouldClose=*/true);
    OS.SetUnbuffered();
    TemplightProtobufWriter Writer(OS, /*aCompressLevel=*/2,
                                   /*aStreaming=*/true, FD);
    Writer.initialize("cut.cpp");
    Writer.printEntry(beginEntry("A"));
    // entries = 2 { end = 2 { time_stamp = 1: 0.0 } }, without its length.
    const char End[] = {0x12, 0x0B, 0x12, 0x09, 0x09, 0, 0, 0, 0, 0, 0, 0, 0};
    ASSERT_EQ(::write(FD, End, sizeof(End)), ssize_t(sizeof(End)));
    Writer.beginDeclRead(declRead("Y"));
    Writer.writeCrashClosing();
  }
  auto Buffer = llvm::MemoryBuffer::getFile(Path);
  llvm::sys::fs::remove(Path);
  ASSERT_TRUE(bool(Buffer));
  std::string Trace = (*Buffer)->getBuffer().str();
  Entries Got = readProtobuf(Trace);
  ASSERT_TRUE(Got.has_value()) << llvm::toHex(Trace);
  EXPECT_EQ(*Got, std::vector<std::string>({"begin A", "end", "unfinished Y"}));
}
#endif

} // namespace

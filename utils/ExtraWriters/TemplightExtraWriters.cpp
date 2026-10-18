//===- TemplightExtraWriters.cpp ----------------*- C++ -*-----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "TemplightExtraWriters.h"

#include <clang/Sema/Sema.h>

#include <llvm/Support/Format.h>
#include <llvm/Support/YAMLTraits.h>
#include <llvm/Support/raw_ostream.h>

namespace clang {

// The names of Sema::CodeSynthesisContext::SynthesisKind, in enum order.
static const char *const SynthesisKindStrings[] = {
    "TemplateInstantiation",
    "DefaultTemplateArgumentInstantiation",
    "DefaultFunctionArgumentInstantiation",
    "ExplicitTemplateArgumentSubstitution",
    "DeducedTemplateArgumentSubstitution",
    "LambdaExpressionSubstitution",
    "PriorTemplateArgumentSubstitution",
    "DefaultTemplateArgumentChecking",
    "ExceptionSpecEvaluation",
    "ExceptionSpecInstantiation",
    "RequirementInstantiation",
    "NestedRequirementConstraintsCheck",
    "DeclaringSpecialMember",
    "DeclaringImplicitEqualityComparison",
    "DefiningSynthesizedFunction",
    "ConstraintsCheck",
    "ConstraintSubstitution",
    "RequirementParameterInstantiation",
    "ParameterMappingSubstitution",
    "RewritingOperatorAsSpaceship",
    "InitializingStructuredBinding",
    "MarkingClassDllexported",
    "BuildingBuiltinDumpStructCall",
    "Memoization",
    "BuildingDeductionGuides",
    "TypeAliasTemplateInstantiation",
    "PartialOrderingTTP",
    "SYCLKernelLaunchLookup",
    "SYCLKernelLaunchOverloadResolution",
    "ExpansionStmtInstantiation"};

static_assert(Sema::CodeSynthesisContext::Memoization == 23 &&
                  Sema::CodeSynthesisContext::ExpansionStmtInstantiation ==
                      sizeof(SynthesisKindStrings) /
                              sizeof(SynthesisKindStrings[0]) -
                          1,
              "SynthesisKindStrings is out of step with clang's SynthesisKind");

static const char *synthesisKindName(int Kind) {
  if (Kind < 0 || static_cast<std::size_t>(Kind) >=
                      sizeof(SynthesisKindStrings) /
                          sizeof(SynthesisKindStrings[0]))
    return "Unknown";
  return SynthesisKindStrings[Kind];
}

static std::string escapeXml(const std::string &Input) {
  std::string Result;
  Result.reserve(64);

  unsigned i, pos = 0;
  for (i = 0; i < Input.length(); ++i) {
    if (Input[i] == '<' || Input[i] == '>' || Input[i] == '"' ||
        Input[i] == '\'' || Input[i] == '&') {
      Result.insert(Result.length(), Input, pos, i - pos);
      pos = i + 1;
      switch (Input[i]) {
      case '<':
        Result += "&lt;";
        break;
      case '>':
        Result += "&gt;";
        break;
      case '\'':
        Result += "&apos;";
        break;
      case '"':
        Result += "&quot;";
        break;
      case '&':
        Result += "&amp;";
        break;
      default:
        break;
      }
    }
  }
  Result.insert(Result.length(), Input, pos, i - pos);
  return Result;
}

} // namespace clang
namespace llvm {
namespace yaml {

template <>
struct ScalarEnumerationTraits<
    clang::Sema::CodeSynthesisContext::SynthesisKind> {
  static void
  enumeration(IO &io, clang::Sema::CodeSynthesisContext::SynthesisKind &value) {

#define def_enum_case(e) io.enumCase(value, clang::synthesisKindName(e), e)

    using CodeSynthesisContext = clang::Sema::CodeSynthesisContext;
    def_enum_case(CodeSynthesisContext::TemplateInstantiation);
    def_enum_case(CodeSynthesisContext::DefaultTemplateArgumentInstantiation);
    def_enum_case(CodeSynthesisContext::DefaultFunctionArgumentInstantiation);
    def_enum_case(CodeSynthesisContext::ExplicitTemplateArgumentSubstitution);
    def_enum_case(CodeSynthesisContext::DeducedTemplateArgumentSubstitution);
    def_enum_case(CodeSynthesisContext::PriorTemplateArgumentSubstitution);
    def_enum_case(CodeSynthesisContext::DefaultTemplateArgumentChecking);
    def_enum_case(CodeSynthesisContext::ExceptionSpecInstantiation);
    def_enum_case(CodeSynthesisContext::DeclaringSpecialMember);
    def_enum_case(CodeSynthesisContext::DefiningSynthesizedFunction);
    def_enum_case(CodeSynthesisContext::Memoization);

#undef def_enum_case
  }
};

template <> struct MappingTraits<clang::PrintableTemplightEntryBegin> {
  static void mapping(IO &io, clang::PrintableTemplightEntryBegin &Entry) {
    bool b = true;
    io.mapRequired("IsBegin", b);
    // must be converted to string before, due to some BS with yaml traits.
    std::string kind = clang::synthesisKindName(Entry.SynthesisKind);
    io.mapRequired("Kind", kind);
    io.mapOptional("Name", Entry.Name);
    std::string loc = Entry.FileName + "|" + std::to_string(Entry.Line) + "|" +
                      std::to_string(Entry.Column);
    io.mapOptional("Location", loc);
    io.mapRequired("TimeStamp", Entry.TimeStamp);
    io.mapOptional("MemoryUsage", Entry.MemoryUsage);
    std::string ori = Entry.TempOri_FileName + "|" +
                      std::to_string(Entry.TempOri_Line) + "|" +
                      std::to_string(Entry.TempOri_Column);
    io.mapOptional("TemplateOrigin", ori);
  }
};

template <> struct MappingTraits<clang::PrintableTemplightEntryEnd> {
  static void mapping(IO &io, clang::PrintableTemplightEntryEnd &Entry) {
    bool b = false;
    io.mapRequired("IsBegin", b);
    io.mapRequired("TimeStamp", Entry.TimeStamp);
    io.mapOptional("MemoryUsage", Entry.MemoryUsage);
  }
};

} // namespace yaml
} // namespace llvm
namespace clang {

// What endSequence() and endDocuments() would write after the entries so far:
// an empty sequence before the first entry, then only the document end.
void TemplightYamlWriter::initialize(const std::string &aSourceName) {
  Output->beginSequence();
  setCrashClosing("\n[]\n...\n");
}

void TemplightYamlWriter::finalize() { Output->endSequence(); }

void TemplightYamlWriter::printEntry(
    const PrintableTemplightEntryBegin &Entry) {
  void *SaveInfo;
  if (Output->preflightElement(1, SaveInfo)) {
    llvm::yaml::EmptyContext Context;
    llvm::yaml::yamlize(*Output,
                        const_cast<PrintableTemplightEntryBegin &>(Entry), true,
                        Context);
    Output->postflightElement(SaveInfo);
  }
  if (!HasEntries) {
    HasEntries = true;
    setCrashClosing("\n...\n");
  }
}

void TemplightYamlWriter::printEntry(const PrintableTemplightEntryEnd &Entry) {
  void *SaveInfo;
  if (Output->preflightElement(1, SaveInfo)) {
    llvm::yaml::EmptyContext Context;
    llvm::yaml::yamlize(*Output,
                        const_cast<PrintableTemplightEntryEnd &>(Entry), true,
                        Context);
    Output->postflightElement(SaveInfo);
  }
  if (!HasEntries) {
    HasEntries = true;
    setCrashClosing("\n...\n");
  }
}

TemplightYamlWriter::TemplightYamlWriter(llvm::raw_ostream &aOS)
    : TemplightWriter(aOS) {
  Output.reset(new llvm::yaml::Output(OutputOS));
  Output->beginDocuments();
}

TemplightYamlWriter::~TemplightYamlWriter() { Output->endDocuments(); }

TemplightXmlWriter::TemplightXmlWriter(llvm::raw_ostream &aOS)
    : TemplightWriter(aOS) {
  OutputOS << "<?xml version=\"1.0\" standalone=\"yes\"?>\n";
}

TemplightXmlWriter::~TemplightXmlWriter() {}

void TemplightXmlWriter::initialize(const std::string &aSourceName) {
  OutputOS << "<Trace>\n";
  setCrashClosing("</Trace>\n");
}

void TemplightXmlWriter::finalize() { OutputOS << "</Trace>\n"; }

void TemplightXmlWriter::printEntry(
    const PrintableTemplightEntryBegin &aEntry) {
  std::string EscapedName = escapeXml(aEntry.Name);
  OutputOS << llvm::format("<TemplateBegin>\n"
                           "    <Kind>%s</Kind>\n"
                           "    <Context context = \"%s\"/>\n"
                           "    <Location>%s|%d|%d</Location>\n",
                           synthesisKindName(aEntry.SynthesisKind),
                           EscapedName.c_str(), aEntry.FileName.c_str(),
                           aEntry.Line, aEntry.Column);
  OutputOS << llvm::format("    <TimeStamp time = \"%.9f\"/>\n"
                           "    <MemoryUsage bytes = \"%d\"/>\n",
                           aEntry.TimeStamp, aEntry.MemoryUsage);
  if (!aEntry.TempOri_FileName.empty()) {
    OutputOS << llvm::format("    <TemplateOrigin>%s|%d|%d</TemplateOrigin>\n",
                             aEntry.TempOri_FileName.c_str(),
                             aEntry.TempOri_Line, aEntry.TempOri_Column);
  }
  OutputOS << "</TemplateBegin>\n";
}

void TemplightXmlWriter::printEntry(const PrintableTemplightEntryEnd &aEntry) {
  OutputOS << llvm::format("<TemplateEnd>\n"
                           "    <TimeStamp time = \"%.9f\"/>\n"
                           "    <MemoryUsage bytes = \"%d\"/>\n"
                           "</TemplateEnd>\n",
                           aEntry.TimeStamp, aEntry.MemoryUsage);
}

TemplightTextWriter::TemplightTextWriter(llvm::raw_ostream &aOS)
    : TemplightWriter(aOS) {}

TemplightTextWriter::~TemplightTextWriter() {}

void TemplightTextWriter::initialize(const std::string &aSourceName) {
  OutputOS << "  SourceFile = " << aSourceName << "\n";
}

void TemplightTextWriter::finalize() {}

void TemplightTextWriter::printEntry(
    const PrintableTemplightEntryBegin &aEntry) {
  OutputOS << llvm::format("TemplateBegin\n"
                           "  Kind = %s\n"
                           "  Name = %s\n"
                           "  Location = %s|%d|%d\n",
                           synthesisKindName(aEntry.SynthesisKind),
                           aEntry.Name.c_str(), aEntry.FileName.c_str(),
                           aEntry.Line, aEntry.Column);
  OutputOS << llvm::format("  TimeStamp = %.9f\n"
                           "  MemoryUsage = %d\n",
                           aEntry.TimeStamp, aEntry.MemoryUsage);
  if (!aEntry.TempOri_FileName.empty()) {
    OutputOS << llvm::format("  TemplateOrigin = %s|%d|%d\n",
                             aEntry.TempOri_FileName.c_str(),
                             aEntry.TempOri_Line, aEntry.TempOri_Column);
  }
}

void TemplightTextWriter::printEntry(const PrintableTemplightEntryEnd &aEntry) {
  OutputOS << llvm::format("TemplateEnd\n"
                           "  TimeStamp = %.9f\n"
                           "  MemoryUsage = %d\n",
                           aEntry.TimeStamp, aEntry.MemoryUsage);
}

struct EntryTraversalTask {
  static const std::size_t invalid_id = ~std::size_t(0);

  PrintableTemplightEntryBegin start;
  PrintableTemplightEntryEnd finish;
  std::size_t nd_id, id_end, parent_id;
  // Streaming: what a crash writes to close this entry while it is open.
  std::string crash_closing;
  EntryTraversalTask(const PrintableTemplightEntryBegin &aStart,
                     std::size_t aNdId, std::size_t aParentId)
      : start(aStart), finish(), nd_id(aNdId), id_end(invalid_id),
        parent_id(aParentId){};
};

struct RecordedDFSEntryTree {
  static const std::size_t invalid_id = ~std::size_t(0);

  std::vector<EntryTraversalTask> parent_stack;
  std::size_t cur_top;

  RecordedDFSEntryTree() : cur_top(invalid_id){};

  void beginEntry(const PrintableTemplightEntryBegin &aEntry) {
    parent_stack.push_back(
        EntryTraversalTask(aEntry, parent_stack.size(),
                           (cur_top == invalid_id ? invalid_id : cur_top)));
    cur_top = parent_stack.size() - 1;
  };

  void endEntry(const PrintableTemplightEntryEnd &aEntry) {
    parent_stack[cur_top].finish = aEntry;
    parent_stack[cur_top].id_end = parent_stack.size();
    if (parent_stack[cur_top].parent_id == invalid_id)
      cur_top = invalid_id;
    else
      cur_top = parent_stack[cur_top].parent_id;
  };
};

TemplightTreeWriter::TemplightTreeWriter(llvm::raw_ostream &aOS,
                                         bool aStreaming)
    : TemplightWriter(aOS), p_tree(new RecordedDFSEntryTree()),
      Streaming(aStreaming) {}

TemplightTreeWriter::~TemplightTreeWriter() {}

void TemplightTreeWriter::printEntry(
    const PrintableTemplightEntryBegin &aEntry) {
  p_tree->beginEntry(aEntry);
  if (Streaming) {
    EntryTraversalTask &Begun = p_tree->parent_stack[p_tree->cur_top];
    streamBegin(Begun);
    Begun.crash_closing = crashClosingOf(Begun);
  }
}

void TemplightTreeWriter::printEntry(const PrintableTemplightEntryEnd &aEntry) {
  std::size_t Ended = p_tree->cur_top;
  p_tree->endEntry(aEntry);
  if (Streaming)
    streamEnd(p_tree->parent_stack[Ended]);
}

void TemplightTreeWriter::initialize(const std::string &aSourceName) {
  this->initializeTree(aSourceName);
  if (Streaming)
    TraceClosing = crashClosingOfTrace();
}

void TemplightTreeWriter::finalize() {
  if (!Streaming) {
    std::vector<std::size_t> open_set;
    std::vector<EntryTraversalTask> &tree = p_tree->parent_stack;

    for (std::size_t i = 0, i_end = tree.size(); i != i_end; ++i) {
      while (!open_set.empty() && (i >= tree[open_set.back()].id_end)) {
        closePrintedTreeNode(tree[open_set.back()]);
        open_set.pop_back();
      }
      openPrintedTreeNode(tree[i]);
      open_set.push_back(i);
    }
    while (!open_set.empty()) {
      closePrintedTreeNode(tree[open_set.back()]);
      open_set.pop_back();
    }
  }

  this->finalizeTree();
}

void TemplightTreeWriter::writeCrashClosing() {
  // Innermost open entry first. Only prepared strings are written here.
  for (std::size_t I = p_tree->cur_top; I != RecordedDFSEntryTree::invalid_id;
       I = p_tree->parent_stack[I].parent_id) {
    const std::string &Closing = p_tree->parent_stack[I].crash_closing;
    OutputOS.write(Closing.data(), Closing.size());
  }
  OutputOS.write(TraceClosing.data(), TraceClosing.size());
}

TemplightNestedXMLWriter::TemplightNestedXMLWriter(llvm::raw_ostream &aOS,
                                                   bool aStreaming)
    : TemplightTreeWriter(aOS, aStreaming) {
  OutputOS << "<?xml version=\"1.0\" standalone=\"yes\"?>\n";
}

TemplightNestedXMLWriter::~TemplightNestedXMLWriter() {}

void TemplightNestedXMLWriter::initializeTree(const std::string &aSourceName) {
  OutputOS << "<Trace>\n";
}

void TemplightNestedXMLWriter::finalizeTree() { OutputOS << "</Trace>\n"; }

// The opening of an <Entry> tag up to the measurements, which only the end of
// the instantiation provides.
static void
printNestedXMLEntryHead(llvm::raw_ostream &OS,
                        const PrintableTemplightEntryBegin &BegEntry) {
  std::string EscapedName = escapeXml(BegEntry.Name);

  OS << llvm::format("<Entry Kind=\"%s\" Name=\"%s\" ",
                     synthesisKindName(BegEntry.SynthesisKind),
                     EscapedName.c_str());
  OS << llvm::format("Location=\"%s|%d|%d\" ", BegEntry.FileName.c_str(),
                     BegEntry.Line, BegEntry.Column);
  if (!BegEntry.TempOri_FileName.empty()) {
    OS << llvm::format("TemplateOrigin=\"%s|%d|%d\" ",
                       BegEntry.TempOri_FileName.c_str(), BegEntry.TempOri_Line,
                       BegEntry.TempOri_Column);
  }
}

void TemplightNestedXMLWriter::openPrintedTreeNode(
    const EntryTraversalTask &aNode) {
  const PrintableTemplightEntryBegin &BegEntry = aNode.start;
  const PrintableTemplightEntryEnd &EndEntry = aNode.finish;

  printNestedXMLEntryHead(OutputOS, BegEntry);
  OutputOS << llvm::format("Time=\"%.9f\" Memory=\"%d\">\n",
                           EndEntry.TimeStamp - BegEntry.TimeStamp,
                           EndEntry.MemoryUsage - BegEntry.MemoryUsage);

  // Print only first part (heading).
}

void TemplightNestedXMLWriter::closePrintedTreeNode(
    const EntryTraversalTask &aNode) {
  OutputOS << "</Entry>\n";
}

// Streaming: the time and memory of an entry, known only at its end, are an
// <Exit> element closing it; an entry cut short by a crash has none.
void TemplightNestedXMLWriter::streamBegin(const EntryTraversalTask &aNode) {
  printNestedXMLEntryHead(OutputOS, aNode.start);
  OutputOS << ">\n";
}

void TemplightNestedXMLWriter::streamEnd(const EntryTraversalTask &aNode) {
  OutputOS << llvm::format("<Exit Time=\"%.9f\" Memory=\"%d\"/>\n",
                           aNode.finish.TimeStamp - aNode.start.TimeStamp,
                           aNode.finish.MemoryUsage - aNode.start.MemoryUsage);
  OutputOS << "</Entry>\n";
}

std::string
TemplightNestedXMLWriter::crashClosingOf(const EntryTraversalTask &aNode) {
  return "</Entry>\n";
}

std::string TemplightNestedXMLWriter::crashClosingOfTrace() {
  return "</Trace>\n";
}

TemplightGraphMLWriter::TemplightGraphMLWriter(llvm::raw_ostream &aOS,
                                               bool aStreaming)
    : TemplightTreeWriter(aOS, aStreaming), last_edge_id(0) {
  OutputOS << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
              "<graphml xmlns=\"http://graphml.graphdrawing.org/xmlns\""
              " xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
              " xsi:schemaLocation=\"http://graphml.graphdrawing.org/xmlns"
              " http://graphml.graphdrawing.org/xmlns/1.0/graphml.xsd\">\n";
  OutputOS << "<key id=\"d0\" for=\"node\" attr.name=\"Kind\" "
              "attr.type=\"string\"/>\n"
              "<key id=\"d1\" for=\"node\" attr.name=\"Name\" "
              "attr.type=\"string\"/>\n"
              "<key id=\"d2\" for=\"node\" attr.name=\"Location\" "
              "attr.type=\"string\"/>\n"
              "<key id=\"d3\" for=\"node\" attr.name=\"Time\" "
              "attr.type=\"double\">\n"
              "<default>0.0</default>\n"
              "</key>\n"
              "<key id=\"d4\" for=\"node\" attr.name=\"Memory\" "
              "attr.type=\"long\">\n"
              "<default>0</default>\n"
              "</key>\n"
              "<key id=\"d5\" for=\"node\" attr.name=\"TemplateOrigin\" "
              "attr.type=\"string\"/>\n";
  // Streaming writes a node when its instantiation begins, before its time
  // and memory are known; they go on the edge from its parent, written when
  // the instantiation ends.
  if (Streaming)
    OutputOS << "<key id=\"d6\" for=\"edge\" attr.name=\"Time\" "
                "attr.type=\"double\">\n"
                "<default>0.0</default>\n"
                "</key>\n"
                "<key id=\"d7\" for=\"edge\" attr.name=\"Memory\" "
                "attr.type=\"long\">\n"
                "<default>0</default>\n"
                "</key>\n";
}

TemplightGraphMLWriter::~TemplightGraphMLWriter() {
  OutputOS << "</graphml>\n";
}

void TemplightGraphMLWriter::initializeTree(const std::string &aSourceName) {
  OutputOS << "<graph>\n";
  // Streaming: the translation unit is the parent of the top-level entries,
  // so that their time and memory have an edge to go on.
  if (Streaming) {
    std::string EscapedSource = escapeXml(aSourceName);
    OutputOS << llvm::format("<node id=\"tu\">\n"
                             "  <data key=\"d0\">TranslationUnit</data>\n"
                             "  <data key=\"d1\">\"%s\"</data>\n"
                             "</node>\n",
                             EscapedSource.c_str());
  }
}

void TemplightGraphMLWriter::finalizeTree() { OutputOS << "</graph>\n"; }

void TemplightGraphMLWriter::openPrintedTreeNode(
    const EntryTraversalTask &aNode) {
  const PrintableTemplightEntryBegin &BegEntry = aNode.start;
  const PrintableTemplightEntryEnd &EndEntry = aNode.finish;

  OutputOS << llvm::format("<node id=\"n%d\">\n", aNode.nd_id);

  printNodeData(BegEntry);
  OutputOS << llvm::format("  <data key=\"d3\">%.9f</data>\n"
                           "  <data key=\"d4\">%d</data>\n",
                           EndEntry.TimeStamp - BegEntry.TimeStamp,
                           EndEntry.MemoryUsage - BegEntry.MemoryUsage);
  if (!BegEntry.TempOri_FileName.empty()) {
    OutputOS << llvm::format("  <data key=\"d5\">\"%s|%d|%d\"</data>\n",
                             BegEntry.TempOri_FileName.c_str(),
                             BegEntry.TempOri_Line, BegEntry.TempOri_Column);
  }

  OutputOS << "</node>\n";
  if (aNode.parent_id == RecordedDFSEntryTree::invalid_id)
    return;

  OutputOS << llvm::format("<edge id=\"e%d\" source=\"n%d\" target=\"n%d\"/>\n",
                           last_edge_id++, aNode.parent_id, aNode.nd_id);
}

void TemplightGraphMLWriter::closePrintedTreeNode(
    const EntryTraversalTask &aNode) {}

void TemplightGraphMLWriter::printNodeData(
    const PrintableTemplightEntryBegin &BegEntry) {
  std::string EscapedName = escapeXml(BegEntry.Name);
  OutputOS << llvm::format("  <data key=\"d0\">%s</data>\n"
                           "  <data key=\"d1\">\"%s\"</data>\n"
                           "  <data key=\"d2\">\"%s|%d|%d\"</data>\n",
                           synthesisKindName(BegEntry.SynthesisKind),
                           EscapedName.c_str(), BegEntry.FileName.c_str(),
                           BegEntry.Line, BegEntry.Column);
}

// The streamed parent of a node: the translation unit for a top-level entry.
static std::string graphMLParentID(const EntryTraversalTask &aNode) {
  if (aNode.parent_id == RecordedDFSEntryTree::invalid_id)
    return "tu";
  return "n" + std::to_string(aNode.parent_id);
}

void TemplightGraphMLWriter::streamBegin(const EntryTraversalTask &aNode) {
  const PrintableTemplightEntryBegin &BegEntry = aNode.start;

  OutputOS << llvm::format("<node id=\"n%d\">\n", aNode.nd_id);
  printNodeData(BegEntry);
  if (!BegEntry.TempOri_FileName.empty()) {
    OutputOS << llvm::format("  <data key=\"d5\">\"%s|%d|%d\"</data>\n",
                             BegEntry.TempOri_FileName.c_str(),
                             BegEntry.TempOri_Line, BegEntry.TempOri_Column);
  }
  OutputOS << "</node>\n";
}

void TemplightGraphMLWriter::streamEnd(const EntryTraversalTask &aNode) {
  std::string Parent = graphMLParentID(aNode);
  OutputOS << llvm::format("<edge id=\"e%d\" source=\"%s\" target=\"n%d\">\n",
                           aNode.nd_id, Parent.c_str(), aNode.nd_id);
  OutputOS << llvm::format("  <data key=\"d6\">%.9f</data>\n"
                           "  <data key=\"d7\">%d</data>\n",
                           aNode.finish.TimeStamp - aNode.start.TimeStamp,
                           aNode.finish.MemoryUsage - aNode.start.MemoryUsage);
  OutputOS << "</edge>\n";
}

std::string
TemplightGraphMLWriter::crashClosingOf(const EntryTraversalTask &aNode) {
  std::string ID = std::to_string(aNode.nd_id);
  return "<edge id=\"e" + ID + "\" source=\"" + graphMLParentID(aNode) +
         "\" target=\"n" + ID + "\"/>\n";
}

std::string TemplightGraphMLWriter::crashClosingOfTrace() {
  return "</graph>\n</graphml>\n";
}

TemplightGraphVizWriter::TemplightGraphVizWriter(llvm::raw_ostream &aOS,
                                                 bool aStreaming)
    : TemplightTreeWriter(aOS, aStreaming) {}

TemplightGraphVizWriter::~TemplightGraphVizWriter() {}

void TemplightGraphVizWriter::initializeTree(const std::string &aSourceName) {
  OutputOS << "digraph Trace {\n";
}

void TemplightGraphVizWriter::finalizeTree() { OutputOS << "}\n"; }

void TemplightGraphVizWriter::printNodeLabel(const EntryTraversalTask &aNode,
                                             bool WithMeasures) {
  const PrintableTemplightEntryBegin &BegEntry = aNode.start;
  const PrintableTemplightEntryEnd &EndEntry = aNode.finish;

  std::string EscapedName = escapeXml(BegEntry.Name);
  OutputOS << llvm::format("n%d [label = ", aNode.nd_id)
           << llvm::format("\"%s\\n"
                           "%s\\n"
                           "At %s Line %d Column %d\\n",
                           synthesisKindName(BegEntry.SynthesisKind),
                           EscapedName.c_str(), BegEntry.FileName.c_str(),
                           BegEntry.Line, BegEntry.Column);
  if (!BegEntry.TempOri_FileName.empty()) {
    OutputOS << llvm::format("From %s Line %d Column %d\\n",
                             BegEntry.TempOri_FileName.c_str(),
                             BegEntry.TempOri_Line, BegEntry.TempOri_Column);
  }
  if (!WithMeasures) {
    OutputOS << "\" ];\n";
    return;
  }
  OutputOS << llvm::format("Time: %.9f seconds Memory: %d bytes\" ];\n",
                           EndEntry.TimeStamp - BegEntry.TimeStamp,
                           EndEntry.MemoryUsage - BegEntry.MemoryUsage);
}

void TemplightGraphVizWriter::openPrintedTreeNode(
    const EntryTraversalTask &aNode) {
  printNodeLabel(aNode, /*WithMeasures=*/true);

  if (aNode.parent_id == RecordedDFSEntryTree::invalid_id)
    return;

  OutputOS << llvm::format("n%d -> n%d;\n", aNode.parent_id, aNode.nd_id);
}

void TemplightGraphVizWriter::closePrintedTreeNode(
    const EntryTraversalTask &aNode) {}

// Streaming: a node is declared when its instantiation begins and declared
// again with its time and memory when it ends; the later label replaces the
// earlier one.
void TemplightGraphVizWriter::streamBegin(const EntryTraversalTask &aNode) {
  printNodeLabel(aNode, /*WithMeasures=*/false);

  if (aNode.parent_id == RecordedDFSEntryTree::invalid_id)
    return;

  OutputOS << llvm::format("n%d -> n%d;\n", aNode.parent_id, aNode.nd_id);
}

void TemplightGraphVizWriter::streamEnd(const EntryTraversalTask &aNode) {
  printNodeLabel(aNode, /*WithMeasures=*/true);
}

std::string
TemplightGraphVizWriter::crashClosingOf(const EntryTraversalTask &aNode) {
  return "";
}

std::string TemplightGraphVizWriter::crashClosingOfTrace() { return "}\n"; }

} // namespace clang

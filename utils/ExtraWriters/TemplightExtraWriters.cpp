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

#include <cstring>

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

namespace {
// -deserialization, under -safe-mode: a read that a crash cut short.
struct UnfinishedDeclRead {
  PrintableTemplightEntryDeclRead Entry;
};
} // namespace

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

template <> struct MappingTraits<clang::PrintableTemplightEntryDeclRead> {
  static void mapping(IO &io, clang::PrintableTemplightEntryDeclRead &Entry) {
    io.mapRequired("DeclRead", Entry.Kind);
    io.mapRequired("Name", Entry.Name);
    io.mapRequired("Module", Entry.ModuleName);
    io.mapRequired("StackDepthKB", Entry.StackDepthKB);
  }
};

template <> struct MappingTraits<clang::PrintableTemplightEntryMergedDecl> {
  static void mapping(IO &io, clang::PrintableTemplightEntryMergedDecl &Entry) {
    io.mapRequired("MergedDecl", Entry.Kind);
    io.mapRequired("Name", Entry.Name);
    io.mapRequired("Modules", Entry.ModuleNames);
  }
};

template <> struct MappingTraits<clang::UnfinishedDeclRead> {
  static void mapping(IO &io, clang::UnfinishedDeclRead &Read) {
    io.mapRequired("UnfinishedDeclRead", Read.Entry.Kind);
    io.mapRequired("Name", Read.Entry.Name);
    io.mapRequired("Module", Read.Entry.ModuleName);
    io.mapRequired("StackDepthKB", Read.Entry.StackDepthKB);
  }
};

} // namespace yaml
} // namespace llvm
namespace clang {

// Writes one element of the sequence \p Output is in.
template <typename EntryT>
static void yamlizeElement(llvm::yaml::Output &Output, EntryT &Entry) {
  void *SaveInfo;
  if (Output.preflightElement(1, SaveInfo)) {
    llvm::yaml::EmptyContext Context;
    llvm::yaml::yamlize(Output, Entry, true, Context);
    Output.postflightElement(SaveInfo);
  }
}

void TemplightYamlWriter::writePending() {
  OutputOS << Pending;
  Pending.clear();
}

void TemplightYamlWriter::initialize(const std::string &aSourceName) {
  Output->beginSequence();
  writePending();
}

void TemplightYamlWriter::finalize() {
  Output->endSequence();
  writePending();
}

template <typename EntryT>
void TemplightYamlWriter::printElement(EntryT &Entry) {
  yamlizeElement(*Output, Entry);
  writePending();
  HasElements.store(true);
}

void TemplightYamlWriter::printEntry(
    const PrintableTemplightEntryBegin &Entry) {
  printElement(const_cast<PrintableTemplightEntryBegin &>(Entry));
}

void TemplightYamlWriter::printEntry(const PrintableTemplightEntryEnd &Entry) {
  printElement(const_cast<PrintableTemplightEntryEnd &>(Entry));
}

void TemplightYamlWriter::printDeclRead(
    const PrintableTemplightEntryDeclRead &Entry) {
  printElement(const_cast<PrintableTemplightEntryDeclRead &>(Entry));
}

void TemplightYamlWriter::printMergedDecl(
    const PrintableTemplightEntryMergedDecl &Entry) {
  printElement(const_cast<PrintableTemplightEntryMergedDecl &>(Entry));
}

// An element of the trace's sequence as Output writes it: a line break, the
// dash and the mapping, with no line break after it.
std::string TemplightYamlWriter::unfinishedDeclReadText(
    const PrintableTemplightEntryDeclRead &Entry) {
  std::string Document;
  {
    llvm::raw_string_ostream OS(Document);
    llvm::yaml::Output Element(OS);
    UnfinishedDeclRead Read{Entry};
    Element.beginDocuments();
    Element.beginSequence();
    yamlizeElement(Element, Read);
  }
  return Document.substr(std::strlen("---"));
}

// What endSequence() and endDocuments() would write after the elements so
// far: an empty sequence before the first, then only the document end.
void TemplightYamlWriter::writeCrashClosing() {
  static const char EmptySequence[] = "\n[]\n...\n";
  static const char DocumentEnd[] = "\n...\n";
  if (writeOpenCrashTexts() || HasElements.load())
    OutputOS.write(DocumentEnd, sizeof(DocumentEnd) - 1);
  else
    OutputOS.write(EmptySequence, sizeof(EmptySequence) - 1);
}

TemplightYamlWriter::TemplightYamlWriter(llvm::raw_ostream &aOS)
    : TemplightWriter(aOS), PendingOS(Pending) {
  Output.reset(new llvm::yaml::Output(PendingOS));
  Output->beginDocuments();
  writePending();
}

TemplightYamlWriter::~TemplightYamlWriter() {
  Output->endDocuments();
  writePending();
}

TemplightXmlWriter::TemplightXmlWriter(llvm::raw_ostream &aOS)
    : TemplightWriter(aOS) {
  OutputOS << "<?xml version=\"1.0\" standalone=\"yes\"?>\n";
  // Until initialize() opens it, a crash writes an empty trace.
  setCrashClosing("<Trace>\n</Trace>\n");
}

TemplightXmlWriter::~TemplightXmlWriter() {}

void TemplightXmlWriter::initialize(const std::string &aSourceName) {
  OutputOS << "<Trace>\n";
  setCrashClosing("</Trace>\n");
}

void TemplightXmlWriter::finalize() { OutputOS << "</Trace>\n"; }

void TemplightXmlWriter::printEntry(
    const PrintableTemplightEntryBegin &aEntry) {
  std::string Element;
  llvm::raw_string_ostream OS(Element);
  std::string EscapedName = escapeXml(aEntry.Name);
  OS << llvm::format("<TemplateBegin>\n"
                     "    <Kind>%s</Kind>\n"
                     "    <Context context = \"%s\"/>\n"
                     "    <Location>%s|%d|%d</Location>\n",
                     synthesisKindName(aEntry.SynthesisKind),
                     EscapedName.c_str(), aEntry.FileName.c_str(), aEntry.Line,
                     aEntry.Column);
  OS << llvm::format("    <TimeStamp time = \"%.9f\"/>\n"
                     "    <MemoryUsage bytes = \"%d\"/>\n",
                     aEntry.TimeStamp, aEntry.MemoryUsage);
  if (!aEntry.TempOri_FileName.empty()) {
    OS << llvm::format("    <TemplateOrigin>%s|%d|%d</TemplateOrigin>\n",
                       aEntry.TempOri_FileName.c_str(), aEntry.TempOri_Line,
                       aEntry.TempOri_Column);
  }
  OS << "</TemplateBegin>\n";
  OutputOS << Element;
}

void TemplightXmlWriter::printEntry(const PrintableTemplightEntryEnd &aEntry) {
  OutputOS << llvm::format("<TemplateEnd>\n"
                           "    <TimeStamp time = \"%.9f\"/>\n"
                           "    <MemoryUsage bytes = \"%d\"/>\n"
                           "</TemplateEnd>\n",
                           aEntry.TimeStamp, aEntry.MemoryUsage);
}

// The <DeclRead> element, or an <UnfinishedDeclRead> one with the same
// children.
static std::string xmlDeclRead(const char *Element,
                               const PrintableTemplightEntryDeclRead &aEntry) {
  std::string Text;
  llvm::raw_string_ostream OS(Text);
  OS << llvm::format("<%s>\n"
                     "    <Kind>%s</Kind>\n"
                     "    <Name>%s</Name>\n"
                     "    <Module>%s</Module>\n"
                     "    <StackDepth kb = \"%llu\"/>\n"
                     "</%s>\n",
                     Element, aEntry.Kind.c_str(),
                     escapeXml(aEntry.Name).c_str(),
                     escapeXml(aEntry.ModuleName).c_str(),
                     (unsigned long long)aEntry.StackDepthKB, Element);
  return Text;
}

void TemplightXmlWriter::printDeclRead(
    const PrintableTemplightEntryDeclRead &aEntry) {
  OutputOS << xmlDeclRead("DeclRead", aEntry);
}

std::string TemplightXmlWriter::unfinishedDeclReadText(
    const PrintableTemplightEntryDeclRead &aEntry) {
  return xmlDeclRead("UnfinishedDeclRead", aEntry);
}

void TemplightXmlWriter::printMergedDecl(
    const PrintableTemplightEntryMergedDecl &aEntry) {
  std::string Element;
  llvm::raw_string_ostream OS(Element);
  OS << llvm::format("<MergedDecl>\n"
                     "    <Kind>%s</Kind>\n"
                     "    <Name>%s</Name>\n",
                     aEntry.Kind.c_str(), escapeXml(aEntry.Name).c_str());
  for (const std::string &ModuleName : aEntry.ModuleNames)
    OS << "    <Module>" << escapeXml(ModuleName) << "</Module>\n";
  OS << "</MergedDecl>\n";
  OutputOS << Element;
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
  std::string Text;
  llvm::raw_string_ostream OS(Text);
  OS << llvm::format("TemplateBegin\n"
                     "  Kind = %s\n"
                     "  Name = %s\n"
                     "  Location = %s|%d|%d\n",
                     synthesisKindName(aEntry.SynthesisKind),
                     aEntry.Name.c_str(), aEntry.FileName.c_str(), aEntry.Line,
                     aEntry.Column);
  OS << llvm::format("  TimeStamp = %.9f\n"
                     "  MemoryUsage = %d\n",
                     aEntry.TimeStamp, aEntry.MemoryUsage);
  if (!aEntry.TempOri_FileName.empty()) {
    OS << llvm::format("  TemplateOrigin = %s|%d|%d\n",
                       aEntry.TempOri_FileName.c_str(), aEntry.TempOri_Line,
                       aEntry.TempOri_Column);
  }
  OutputOS << Text;
}

void TemplightTextWriter::printEntry(const PrintableTemplightEntryEnd &aEntry) {
  OutputOS << llvm::format("TemplateEnd\n"
                           "  TimeStamp = %.9f\n"
                           "  MemoryUsage = %d\n",
                           aEntry.TimeStamp, aEntry.MemoryUsage);
}

// The DeclRead entry, or an UnfinishedDeclRead one with the same fields.
static std::string textDeclRead(const char *Entry,
                                const PrintableTemplightEntryDeclRead &aEntry) {
  std::string Text;
  llvm::raw_string_ostream OS(Text);
  OS << llvm::format("%s\n"
                     "  Kind = %s\n"
                     "  Name = %s\n"
                     "  Module = %s\n"
                     "  StackDepthKB = %llu\n",
                     Entry, aEntry.Kind.c_str(), aEntry.Name.c_str(),
                     aEntry.ModuleName.c_str(),
                     (unsigned long long)aEntry.StackDepthKB);
  return Text;
}

void TemplightTextWriter::printDeclRead(
    const PrintableTemplightEntryDeclRead &aEntry) {
  OutputOS << textDeclRead("DeclRead", aEntry);
}

std::string TemplightTextWriter::unfinishedDeclReadText(
    const PrintableTemplightEntryDeclRead &aEntry) {
  return textDeclRead("UnfinishedDeclRead", aEntry);
}

void TemplightTextWriter::printMergedDecl(
    const PrintableTemplightEntryMergedDecl &aEntry) {
  std::string Text;
  llvm::raw_string_ostream OS(Text);
  OS << llvm::format("MergedDecl\n"
                     "  Kind = %s\n"
                     "  Name = %s\n",
                     aEntry.Kind.c_str(), aEntry.Name.c_str());
  for (const std::string &ModuleName : aEntry.ModuleNames)
    OS << "  Module = " << ModuleName << "\n";
  OutputOS << Text;
}

struct EntryTraversalTask {
  static const std::size_t invalid_id = ~std::size_t(0);

  PrintableTemplightEntryBegin start;
  PrintableTemplightEntryEnd finish;
  std::size_t nd_id, id_end, parent_id;
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
    const EntryTraversalTask &Begun = p_tree->parent_stack[p_tree->cur_top];
    std::string Opening = streamedBegin(Begun);
    CrashText *Closing = prepareCrashText(crashClosingOf(Begun));
    OutputOS << Opening;
    publishCrashText(Closing);
  }
}

void TemplightTreeWriter::printEntry(const PrintableTemplightEntryEnd &aEntry) {
  std::size_t Ended = p_tree->cur_top;
  p_tree->endEntry(aEntry);
  if (Streaming) {
    OutputOS << streamedEnd(p_tree->parent_stack[Ended]);
    retireCrashText();
  }
}

void TemplightTreeWriter::initialize(const std::string &aSourceName) {
  this->initializeTree(aSourceName);
  if (Streaming)
    setCrashClosing(crashClosingOfTrace());
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

TemplightNestedXMLWriter::TemplightNestedXMLWriter(llvm::raw_ostream &aOS,
                                                   bool aStreaming)
    : TemplightTreeWriter(aOS, aStreaming) {
  OutputOS << "<?xml version=\"1.0\" standalone=\"yes\"?>\n";
  // Until initialize() opens it, a crash writes an empty trace.
  if (Streaming)
    setCrashClosing("<Trace>\n</Trace>\n");
}

TemplightNestedXMLWriter::~TemplightNestedXMLWriter() {}

void TemplightNestedXMLWriter::initializeTree(const std::string &aSourceName) {
  OutputOS << "<Trace>\n";
}

void TemplightNestedXMLWriter::finalizeTree() {
  OutputOS << PendingDecls << "</Trace>\n";
}

// The <DeclRead> element, or an <UnfinishedDeclRead> one with the same
// attributes.
static std::string
nestedXMLDeclRead(const char *Element,
                  const PrintableTemplightEntryDeclRead &aEntry) {
  std::string Text;
  llvm::raw_string_ostream OS(Text);
  OS << llvm::format("<%s Kind=\"%s\" Name=\"%s\" Module=\"%s\" "
                     "StackDepthKB=\"%llu\"/>\n",
                     Element, aEntry.Kind.c_str(),
                     escapeXml(aEntry.Name).c_str(),
                     escapeXml(aEntry.ModuleName).c_str(),
                     (unsigned long long)aEntry.StackDepthKB);
  return Text;
}

void TemplightNestedXMLWriter::printDeclRead(
    const PrintableTemplightEntryDeclRead &aEntry) {
  std::string Element = nestedXMLDeclRead("DeclRead", aEntry);
  if (Streaming)
    OutputOS << Element;
  else
    PendingDecls += Element;
}

std::string TemplightNestedXMLWriter::unfinishedDeclReadText(
    const PrintableTemplightEntryDeclRead &aEntry) {
  return nestedXMLDeclRead("UnfinishedDeclRead", aEntry);
}

void TemplightNestedXMLWriter::printMergedDecl(
    const PrintableTemplightEntryMergedDecl &aEntry) {
  std::string Element;
  llvm::raw_string_ostream OS(Element);
  OS << llvm::format("<MergedDecl Kind=\"%s\" Name=\"%s\">\n",
                     aEntry.Kind.c_str(), escapeXml(aEntry.Name).c_str());
  for (const std::string &ModuleName : aEntry.ModuleNames)
    OS << "<Module Name=\"" << escapeXml(ModuleName) << "\"/>\n";
  OS << "</MergedDecl>\n";
  if (Streaming)
    OutputOS << Element;
  else
    PendingDecls += Element;
}

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
std::string
TemplightNestedXMLWriter::streamedBegin(const EntryTraversalTask &aNode) {
  std::string Text;
  llvm::raw_string_ostream OS(Text);
  printNestedXMLEntryHead(OS, aNode.start);
  OS << ">\n";
  return Text;
}

std::string
TemplightNestedXMLWriter::streamedEnd(const EntryTraversalTask &aNode) {
  std::string Text;
  llvm::raw_string_ostream OS(Text);
  OS << llvm::format("<Exit Time=\"%.9f\" Memory=\"%d\"/>\n",
                     aNode.finish.TimeStamp - aNode.start.TimeStamp,
                     aNode.finish.MemoryUsage - aNode.start.MemoryUsage);
  OS << "</Entry>\n";
  return Text;
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
              "attr.type=\"string\"/>\n"
              "<key id=\"d8\" for=\"node\" attr.name=\"Module\" "
              "attr.type=\"string\"/>\n"
              "<key id=\"d9\" for=\"node\" attr.name=\"StackDepthKB\" "
              "attr.type=\"long\"/>\n";
  // Streaming writes a node when its instantiation begins, before its time
  // and memory are known; they go on the edge from its parent, written when
  // the instantiation ends.
  // A crash marks the declaration reads it cut short.
  if (Streaming)
    OutputOS << "<key id=\"d6\" for=\"edge\" attr.name=\"Time\" "
                "attr.type=\"double\">\n"
                "<default>0.0</default>\n"
                "</key>\n"
                "<key id=\"d7\" for=\"edge\" attr.name=\"Memory\" "
                "attr.type=\"long\">\n"
                "<default>0</default>\n"
                "</key>\n"
                "<key id=\"d10\" for=\"node\" attr.name=\"Unfinished\" "
                "attr.type=\"boolean\"/>\n";
  // Until initialize() opens the graph, a crash writes no graph.
  if (Streaming)
    setCrashClosing("</graphml>\n");
}

TemplightGraphMLWriter::~TemplightGraphMLWriter() {
  OutputOS << "</graphml>\n";
}

void TemplightGraphMLWriter::initializeTree(const std::string &aSourceName) {
  std::string Text = "<graph>\n";
  llvm::raw_string_ostream OS(Text);
  // Streaming: the translation unit is the parent of the top-level entries,
  // so that their time and memory have an edge to go on.
  if (Streaming) {
    std::string EscapedSource = escapeXml(aSourceName);
    OS << llvm::format("<node id=\"tu\">\n"
                       "  <data key=\"d0\">TranslationUnit</data>\n"
                       "  <data key=\"d1\">\"%s\"</data>\n"
                       "</node>\n",
                       EscapedSource.c_str());
  }
  OutputOS << Text;
}

void TemplightGraphMLWriter::finalizeTree() {
  OutputOS << PendingDecls << "</graph>\n";
}

void TemplightGraphMLWriter::printDeclRead(
    const PrintableTemplightEntryDeclRead &aEntry) {
  std::string Node;
  llvm::raw_string_ostream OS(Node);
  OS << llvm::format("<node id=\"r%zu\">\n"
                     "  <data key=\"d0\">%s</data>\n"
                     "  <data key=\"d1\">\"%s\"</data>\n"
                     "  <data key=\"d8\">%s</data>\n"
                     "  <data key=\"d9\">%llu</data>\n"
                     "</node>\n",
                     DeclNodes++, aEntry.Kind.c_str(),
                     escapeXml(aEntry.Name).c_str(),
                     escapeXml(aEntry.ModuleName).c_str(),
                     (unsigned long long)aEntry.StackDepthKB);
  if (Streaming)
    OutputOS << Node;
  else
    PendingDecls += Node;
}

std::string TemplightGraphMLWriter::unfinishedDeclReadText(
    const PrintableTemplightEntryDeclRead &aEntry) {
  std::string Node;
  llvm::raw_string_ostream OS(Node);
  OS << llvm::format("<node id=\"u%zu\">\n"
                     "  <data key=\"d0\">%s</data>\n"
                     "  <data key=\"d1\">\"%s\"</data>\n"
                     "  <data key=\"d8\">%s</data>\n"
                     "  <data key=\"d9\">%llu</data>\n"
                     "  <data key=\"d10\">true</data>\n"
                     "</node>\n",
                     UnfinishedNodes++, aEntry.Kind.c_str(),
                     escapeXml(aEntry.Name).c_str(),
                     escapeXml(aEntry.ModuleName).c_str(),
                     (unsigned long long)aEntry.StackDepthKB);
  return Node;
}

void TemplightGraphMLWriter::printMergedDecl(
    const PrintableTemplightEntryMergedDecl &aEntry) {
  std::string Modules;
  for (const std::string &ModuleName : aEntry.ModuleNames)
    Modules += (Modules.empty() ? "" : ",") + escapeXml(ModuleName);
  std::string Node;
  llvm::raw_string_ostream OS(Node);
  OS << llvm::format("<node id=\"m%zu\">\n"
                     "  <data key=\"d0\">%s</data>\n"
                     "  <data key=\"d1\">\"%s\"</data>\n"
                     "  <data key=\"d8\">%s</data>\n"
                     "</node>\n",
                     DeclNodes++, aEntry.Kind.c_str(),
                     escapeXml(aEntry.Name).c_str(), Modules.c_str());
  if (Streaming)
    OutputOS << Node;
  else
    PendingDecls += Node;
}

void TemplightGraphMLWriter::openPrintedTreeNode(
    const EntryTraversalTask &aNode) {
  const PrintableTemplightEntryBegin &BegEntry = aNode.start;
  const PrintableTemplightEntryEnd &EndEntry = aNode.finish;

  OutputOS << llvm::format("<node id=\"n%d\">\n", aNode.nd_id);

  printNodeData(OutputOS, BegEntry);
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
    llvm::raw_ostream &OS, const PrintableTemplightEntryBegin &BegEntry) {
  std::string EscapedName = escapeXml(BegEntry.Name);
  OS << llvm::format("  <data key=\"d0\">%s</data>\n"
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

std::string
TemplightGraphMLWriter::streamedBegin(const EntryTraversalTask &aNode) {
  const PrintableTemplightEntryBegin &BegEntry = aNode.start;

  std::string Text;
  llvm::raw_string_ostream OS(Text);
  OS << llvm::format("<node id=\"n%d\">\n", aNode.nd_id);
  printNodeData(OS, BegEntry);
  if (!BegEntry.TempOri_FileName.empty()) {
    OS << llvm::format("  <data key=\"d5\">\"%s|%d|%d\"</data>\n",
                       BegEntry.TempOri_FileName.c_str(), BegEntry.TempOri_Line,
                       BegEntry.TempOri_Column);
  }
  OS << "</node>\n";
  return Text;
}

std::string
TemplightGraphMLWriter::streamedEnd(const EntryTraversalTask &aNode) {
  std::string Parent = graphMLParentID(aNode);
  std::string Text;
  llvm::raw_string_ostream OS(Text);
  OS << llvm::format("<edge id=\"e%d\" source=\"%s\" target=\"n%d\">\n",
                     aNode.nd_id, Parent.c_str(), aNode.nd_id);
  OS << llvm::format("  <data key=\"d6\">%.9f</data>\n"
                     "  <data key=\"d7\">%d</data>\n",
                     aNode.finish.TimeStamp - aNode.start.TimeStamp,
                     aNode.finish.MemoryUsage - aNode.start.MemoryUsage);
  OS << "</edge>\n";
  return Text;
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
    : TemplightTreeWriter(aOS, aStreaming) {
  // Until initialize() opens it, a crash writes an empty graph.
  if (Streaming)
    setCrashClosing("digraph Trace {\n}\n");
}

TemplightGraphVizWriter::~TemplightGraphVizWriter() {}

void TemplightGraphVizWriter::initializeTree(const std::string &aSourceName) {
  OutputOS << "digraph Trace {\n";
}

void TemplightGraphVizWriter::finalizeTree() {
  OutputOS << PendingDecls << "}\n";
}

void TemplightGraphVizWriter::writeDeclNode(const std::string &Node) {
  if (Streaming)
    OutputOS << Node;
  else
    PendingDecls += Node;
}

void TemplightGraphVizWriter::printDeclRead(
    const PrintableTemplightEntryDeclRead &aEntry) {
  std::string Node;
  llvm::raw_string_ostream OS(Node);
  OS << llvm::format("d%zu [shape = note, label = \"DeclRead\\n%s\\n%s\\n"
                     "Module: %s\\nStack: %llu KB\" ];\n",
                     DeclNodes++, aEntry.Kind.c_str(),
                     escapeXml(aEntry.Name).c_str(),
                     escapeXml(aEntry.ModuleName).c_str(),
                     (unsigned long long)aEntry.StackDepthKB);
  writeDeclNode(Node);
}

std::string TemplightGraphVizWriter::unfinishedDeclReadText(
    const PrintableTemplightEntryDeclRead &aEntry) {
  std::string Node;
  llvm::raw_string_ostream OS(Node);
  OS << llvm::format("u%zu [shape = note, label = \"UnfinishedDeclRead\\n%s\\n"
                     "%s\\nModule: %s\\nStack: %llu KB\" ];\n",
                     UnfinishedNodes++, aEntry.Kind.c_str(),
                     escapeXml(aEntry.Name).c_str(),
                     escapeXml(aEntry.ModuleName).c_str(),
                     (unsigned long long)aEntry.StackDepthKB);
  return Node;
}

void TemplightGraphVizWriter::printMergedDecl(
    const PrintableTemplightEntryMergedDecl &aEntry) {
  std::string Node;
  llvm::raw_string_ostream OS(Node);
  OS << llvm::format("d%zu [shape = note, label = \"MergedDecl\\n%s\\n%s",
                     DeclNodes++, aEntry.Kind.c_str(),
                     escapeXml(aEntry.Name).c_str());
  for (const std::string &ModuleName : aEntry.ModuleNames)
    OS << "\\nModule: " << escapeXml(ModuleName);
  OS << "\" ];\n";
  writeDeclNode(Node);
}

void TemplightGraphVizWriter::printNodeLabel(llvm::raw_ostream &OS,
                                             const EntryTraversalTask &aNode,
                                             bool WithMeasures) {
  const PrintableTemplightEntryBegin &BegEntry = aNode.start;
  const PrintableTemplightEntryEnd &EndEntry = aNode.finish;

  std::string EscapedName = escapeXml(BegEntry.Name);
  OS << llvm::format("n%d [label = ", aNode.nd_id)
     << llvm::format("\"%s\\n"
                     "%s\\n"
                     "At %s Line %d Column %d\\n",
                     synthesisKindName(BegEntry.SynthesisKind),
                     EscapedName.c_str(), BegEntry.FileName.c_str(),
                     BegEntry.Line, BegEntry.Column);
  if (!BegEntry.TempOri_FileName.empty()) {
    OS << llvm::format("From %s Line %d Column %d\\n",
                       BegEntry.TempOri_FileName.c_str(), BegEntry.TempOri_Line,
                       BegEntry.TempOri_Column);
  }
  if (!WithMeasures) {
    OS << "\" ];\n";
    return;
  }
  OS << llvm::format("Time: %.9f seconds Memory: %d bytes\" ];\n",
                     EndEntry.TimeStamp - BegEntry.TimeStamp,
                     EndEntry.MemoryUsage - BegEntry.MemoryUsage);
}

void TemplightGraphVizWriter::openPrintedTreeNode(
    const EntryTraversalTask &aNode) {
  printNodeLabel(OutputOS, aNode, /*WithMeasures=*/true);

  if (aNode.parent_id == RecordedDFSEntryTree::invalid_id)
    return;

  OutputOS << llvm::format("n%d -> n%d;\n", aNode.parent_id, aNode.nd_id);
}

void TemplightGraphVizWriter::closePrintedTreeNode(
    const EntryTraversalTask &aNode) {}

// Streaming: a node is declared when its instantiation begins and declared
// again with its time and memory when it ends; the later label replaces the
// earlier one.
std::string
TemplightGraphVizWriter::streamedBegin(const EntryTraversalTask &aNode) {
  std::string Text;
  llvm::raw_string_ostream OS(Text);
  printNodeLabel(OS, aNode, /*WithMeasures=*/false);
  if (aNode.parent_id != RecordedDFSEntryTree::invalid_id)
    OS << llvm::format("n%d -> n%d;\n", aNode.parent_id, aNode.nd_id);
  return Text;
}

std::string
TemplightGraphVizWriter::streamedEnd(const EntryTraversalTask &aNode) {
  std::string Text;
  llvm::raw_string_ostream OS(Text);
  printNodeLabel(OS, aNode, /*WithMeasures=*/true);
  return Text;
}

std::string
TemplightGraphVizWriter::crashClosingOf(const EntryTraversalTask &aNode) {
  return "";
}

std::string TemplightGraphVizWriter::crashClosingOfTrace() { return "}\n"; }

} // namespace clang

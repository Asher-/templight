//===- TemplightProtobufWriter.h --------------------*- C++ -*-------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TEMPLIGHT_EXTRA_WRITERS_H
#define LLVM_CLANG_TEMPLIGHT_EXTRA_WRITERS_H

#include "PrintableTemplightEntries.h"

#include <llvm/Support/raw_ostream.h>

#include <memory>
#include <string>

namespace llvm {
namespace yaml {
class Output;
}
} // namespace llvm

namespace clang {

class TemplightYamlWriter : public TemplightWriter {
public:
  TemplightYamlWriter(llvm::raw_ostream &aOS);
  ~TemplightYamlWriter();

  void initialize(const std::string &aSourceName = "") override;
  void finalize() override;

  void printEntry(const PrintableTemplightEntryBegin &aEntry) override;
  void printEntry(const PrintableTemplightEntryEnd &aEntry) override;
  void printDeclRead(const PrintableTemplightEntryDeclRead &aEntry) override;
  void
  printMergedDecl(const PrintableTemplightEntryMergedDecl &aEntry) override;

  void writeCrashClosing() override;

protected:
  std::string unfinishedDeclReadText(
      const PrintableTemplightEntryDeclRead &aEntry) override;

private:
  // Writes one sequence element, as printEntry does.
  template <typename EntryT> void printElement(EntryT &Entry);
  // Writes what Output wrote since the last call, with one write.
  void writePending();
  std::string Pending;
  llvm::raw_string_ostream PendingOS;
  std::unique_ptr<llvm::yaml::Output> Output;
  std::atomic<bool> HasElements{false};
};

class TemplightXmlWriter : public TemplightWriter {
public:
  TemplightXmlWriter(llvm::raw_ostream &aOS);
  ~TemplightXmlWriter();

  void initialize(const std::string &aSourceName = "") override;
  void finalize() override;

  void printEntry(const PrintableTemplightEntryBegin &aEntry) override;
  void printEntry(const PrintableTemplightEntryEnd &aEntry) override;
  void printDeclRead(const PrintableTemplightEntryDeclRead &aEntry) override;
  void
  printMergedDecl(const PrintableTemplightEntryMergedDecl &aEntry) override;

protected:
  std::string unfinishedDeclReadText(
      const PrintableTemplightEntryDeclRead &aEntry) override;
};

class TemplightTextWriter : public TemplightWriter {
public:
  TemplightTextWriter(llvm::raw_ostream &aOS);
  ~TemplightTextWriter();

  void initialize(const std::string &aSourceName = "") override;
  void finalize() override;

  void printEntry(const PrintableTemplightEntryBegin &aEntry) override;
  void printEntry(const PrintableTemplightEntryEnd &aEntry) override;
  void printDeclRead(const PrintableTemplightEntryDeclRead &aEntry) override;
  void
  printMergedDecl(const PrintableTemplightEntryMergedDecl &aEntry) override;

protected:
  std::string unfinishedDeclReadText(
      const PrintableTemplightEntryDeclRead &aEntry) override;
};

struct RecordedDFSEntryTree;
struct EntryTraversalTask;

/// Writes the trace as a tree of instantiations. By default the tree is
/// printed at finalize(); when streaming (safe-mode), each entry is written
/// as it arrives and a crash closes the entries still open.
class TemplightTreeWriter : public TemplightWriter {
public:
  TemplightTreeWriter(llvm::raw_ostream &aOS, bool aStreaming = false);
  ~TemplightTreeWriter();

  void initialize(const std::string &aSourceName = "") override;
  void finalize() override;

  void printEntry(const PrintableTemplightEntryBegin &aEntry) override;
  void printEntry(const PrintableTemplightEntryEnd &aEntry) override;

protected:
  virtual void openPrintedTreeNode(const EntryTraversalTask &aNode) = 0;
  virtual void closePrintedTreeNode(const EntryTraversalTask &aNode) = 0;

  virtual void initializeTree(const std::string &aSourceName) = 0;
  virtual void finalizeTree() = 0;

  /// Streaming: what is written for a node when its instantiation begins,
  /// and what is known only at its end (time, memory) when it ends.
  virtual std::string streamedBegin(const EntryTraversalTask &aNode) = 0;
  virtual std::string streamedEnd(const EntryTraversalTask &aNode) = 0;
  /// Streaming: what a crash writes to close a node still open, and then to
  /// close the trace.
  virtual std::string crashClosingOf(const EntryTraversalTask &aNode) = 0;
  virtual std::string crashClosingOfTrace() = 0;

  std::unique_ptr<RecordedDFSEntryTree> p_tree;
  bool Streaming;
};

class TemplightNestedXMLWriter : public TemplightTreeWriter {
public:
  TemplightNestedXMLWriter(llvm::raw_ostream &aOS, bool aStreaming = false);
  ~TemplightNestedXMLWriter();

  void printDeclRead(const PrintableTemplightEntryDeclRead &aEntry) override;
  void
  printMergedDecl(const PrintableTemplightEntryMergedDecl &aEntry) override;

protected:
  void openPrintedTreeNode(const EntryTraversalTask &aNode) override;
  void closePrintedTreeNode(const EntryTraversalTask &aNode) override;

  void initializeTree(const std::string &aSourceName = "") override;
  void finalizeTree() override;

  std::string streamedBegin(const EntryTraversalTask &aNode) override;
  std::string streamedEnd(const EntryTraversalTask &aNode) override;
  std::string crashClosingOf(const EntryTraversalTask &aNode) override;
  std::string crashClosingOfTrace() override;
  std::string unfinishedDeclReadText(
      const PrintableTemplightEntryDeclRead &aEntry) override;

private:
  // -deserialization elements held until finalizeTree() when not streaming.
  std::string PendingDecls;
};

class TemplightGraphMLWriter : public TemplightTreeWriter {
public:
  TemplightGraphMLWriter(llvm::raw_ostream &aOS, bool aStreaming = false);
  ~TemplightGraphMLWriter();

  void printDeclRead(const PrintableTemplightEntryDeclRead &aEntry) override;
  void
  printMergedDecl(const PrintableTemplightEntryMergedDecl &aEntry) override;

protected:
  void openPrintedTreeNode(const EntryTraversalTask &aNode) override;
  void closePrintedTreeNode(const EntryTraversalTask &aNode) override;

  void initializeTree(const std::string &aSourceName = "") override;
  void finalizeTree() override;

  std::string streamedBegin(const EntryTraversalTask &aNode) override;
  std::string streamedEnd(const EntryTraversalTask &aNode) override;
  std::string crashClosingOf(const EntryTraversalTask &aNode) override;
  std::string crashClosingOfTrace() override;
  std::string unfinishedDeclReadText(
      const PrintableTemplightEntryDeclRead &aEntry) override;

private:
  // -deserialization nodes held until finalizeTree() when not streaming.
  std::string PendingDecls;
  std::size_t DeclNodes = 0;
  std::size_t UnfinishedNodes = 0;
  void printNodeData(llvm::raw_ostream &OS,
                     const PrintableTemplightEntryBegin &BegEntry);

  int last_edge_id;
};

class TemplightGraphVizWriter : public TemplightTreeWriter {
public:
  TemplightGraphVizWriter(llvm::raw_ostream &aOS, bool aStreaming = false);
  ~TemplightGraphVizWriter();

  void printDeclRead(const PrintableTemplightEntryDeclRead &aEntry) override;
  void
  printMergedDecl(const PrintableTemplightEntryMergedDecl &aEntry) override;

protected:
  void openPrintedTreeNode(const EntryTraversalTask &aNode) override;
  void closePrintedTreeNode(const EntryTraversalTask &aNode) override;

  void initializeTree(const std::string &aSourceName = "") override;
  void finalizeTree() override;

  std::string streamedBegin(const EntryTraversalTask &aNode) override;
  std::string streamedEnd(const EntryTraversalTask &aNode) override;
  std::string crashClosingOf(const EntryTraversalTask &aNode) override;
  std::string crashClosingOfTrace() override;
  std::string unfinishedDeclReadText(
      const PrintableTemplightEntryDeclRead &aEntry) override;

private:
  void printNodeLabel(llvm::raw_ostream &OS, const EntryTraversalTask &aNode,
                      bool WithMeasures);

private:
  // -deserialization nodes held until finalizeTree() when not streaming.
  std::string PendingDecls;
  std::size_t DeclNodes = 0;
  std::size_t UnfinishedNodes = 0;
  void writeDeclNode(const std::string &Node);
};

/*
class ProtobufPrinter : public TemplightTracer::TracePrinter {
protected:
  void startTraceImpl() override {
    // get the source name from the source manager:
    FileID fileID = TheSema.getSourceManager().getMainFileID();
    std::string src_name =
      TheSema.getSourceManager().getFileEntryForID(fileID)->getName();
    Writer.initialize(src_name);
  };
  void endTraceImpl() override {
    Writer.finalize();
    Writer.dumpOnStream(*this->TraceOS);
  };

  void printEntryImpl(const PrintableTemplightEntryBegin& Entry) override {
    Writer.printEntry(Entry);
  };

  void printEntryImpl(const PrintableTemplightEntryEnd& Entry) override {
    Writer.printEntry(Entry);
  };

public:

  ProtobufPrinter(const Sema &aSema, const std::string &Output) :
                  TemplightTracer::TracePrinter(aSema, Output) { };

private:
  TemplightProtobufWriter Writer;
};
*/

} // namespace clang

#endif

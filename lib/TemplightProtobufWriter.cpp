//===- TemplightProtobufWriter.cpp ------------*- C++ -*-------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "TemplightProtobufWriter.h"
#include "PrintableTemplightEntries.h"

#include "ThinProtobuf.h"
#include "llvm/ADT/StringExtras.h"

#include <llvm/Config/llvm-config.h>
#include <llvm/Support/Compression.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/raw_ostream.h>

#ifdef LLVM_ON_UNIX
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <cstdint>
#include <string>

namespace clang {

// Width of the varint holding the streamed trace record's length. Five bytes
// hold lengths below 2^35, and keep every 7-bit group within the shifts that
// llvm::protobuf::loadVarInt evaluates in int.
static const unsigned StreamedLengthWidth = 5;

TemplightProtobufWriter::TemplightProtobufWriter(llvm::raw_ostream &aOS,
                                                 int aCompressLevel,
                                                 bool aStreaming, int aFD)
    : TemplightWriter(aOS), compressionMode(aCompressLevel),
      Streaming(aStreaming), FD(aFD) {}

std::string TemplightProtobufWriter::traceBytes(llvm::StringRef Field) const {
  if (Seekable)
    return Field.str();
  // repeated TemplightTrace traces = 1; one record per append, which
  // TemplightProtobufReader reads on as the same trace.
  std::string Record;
  llvm::raw_string_ostream OS(Record);
  llvm::protobuf::saveString(OS, 1, Field);
  return Record;
}

void TemplightProtobufWriter::appendToTrace(llvm::StringRef Field) {
  if (!Streaming) {
    buffer.append(Field.data(), Field.size());
    return;
  }
  OutputOS << traceBytes(Field);
  OutputOS.flush();
  if (!Seekable)
    return;
  BodySize += Field.size();
  patchLength(BodySize);
}

// Called from the crash handler too, so it only computes and writes.
void TemplightProtobufWriter::patchLength(std::uint64_t Size) {
  if (Size >> (7 * StreamedLengthWidth))
    return; // Too long for the field: the last written length stays valid.
  char Length[StreamedLengthWidth];
  for (unsigned I = 0; I < StreamedLengthWidth; ++I)
    Length[I] = static_cast<char>(((Size >> (7 * I)) & 0x7F) |
                                  (I + 1 < StreamedLengthWidth ? 0x80 : 0));
#ifdef LLVM_ON_UNIX
  // pwrite leaves the file offset where it is, so a crash amid it cannot
  // misplace what the crash closing appends.
  if (FD >= 0) {
    (void)::pwrite(FD, Length, StreamedLengthWidth, LengthOffset);
    return;
  }
#endif
  static_cast<llvm::raw_fd_ostream &>(OutputOS).pwrite(
      Length, StreamedLengthWidth, LengthOffset);
}

void TemplightProtobufWriter::writeCrashClosing() {
  TemplightWriter::writeCrashClosing();
#ifdef LLVM_ON_UNIX
  // Each field is written with one write, so the file ends with a whole one.
  struct stat Status;
  if (Streaming && Seekable && FD >= 0 && ::fstat(FD, &Status) == 0 &&
      std::uint64_t(Status.st_size) >= LengthOffset + StreamedLengthWidth)
    patchLength(Status.st_size - LengthOffset - StreamedLengthWidth);
#endif
}

void TemplightProtobufWriter::initialize(const std::string &aSourceName) {

  std::string hdr_contents;
  {
    llvm::raw_string_ostream OS_inner(hdr_contents);

    /*
  message TemplightHeader {
    required uint32 version = 1;
    optional string source_file = 2;
  }
    */

    llvm::protobuf::saveVarInt(OS_inner, 1, 1); // version
    if (!aSourceName.empty())
      llvm::protobuf::saveString(OS_inner, 2, aSourceName); // source_file
  }

  std::string hdr_field;
  {
    llvm::raw_string_ostream OS(hdr_field);

    // required TemplightHeader header = 1;
    llvm::protobuf::saveString(OS, 1, hdr_contents);
  }

  if (Streaming) {
    // A descriptor comes only with the raw_fd_ostream that writes its file;
    // raw_fd_ostream itself reports OK_OStream, so its kind cannot tell.
    Seekable = FD >= 0 &&
               static_cast<llvm::raw_fd_ostream &>(OutputOS).supportsSeeking();
    if (Seekable) {
      // repeated TemplightTrace traces = 1; its length is patched in place.
      std::string Head;
      llvm::raw_string_ostream OS(Head);
      llvm::protobuf::saveVarInt(OS, llvm::protobuf::getStringWire<1>::value);
      LengthOffset = OutputOS.tell() + Head.size();
      OS << std::string(StreamedLengthWidth, '\0');
      OutputOS << Head;
      patchLength(BodySize);
    }
  }
  appendToTrace(hdr_field);
}

void TemplightProtobufWriter::finalize() {
  if (Streaming) {
    OutputOS.flush();
    return;
  }
  // repeated TemplightTrace traces = 1;
  llvm::protobuf::saveString(OutputOS, 1, buffer);
}

std::string
TemplightProtobufWriter::printEntryLocation(const std::string &FileName,
                                            int Line, int Column) {

  /*
  message SourceLocation {
    optional string file_name = 1;
    required uint32 file_id = 2;
    required uint32 line = 3;
    optional uint32 column = 4;
  }
  */

  std::string location_contents;
  llvm::raw_string_ostream OS_inner(location_contents);

  std::unordered_map<std::string, std::size_t>::iterator it =
      fileNameMap.find(FileName);

  if (it == fileNameMap.end()) {
    llvm::protobuf::saveString(OS_inner, 1, FileName); // file_name
    std::size_t file_id = fileNameMap.size();
    llvm::protobuf::saveVarInt(OS_inner, 2, file_id); // file_id
    fileNameMap[FileName] = file_id;
  } else {
    llvm::protobuf::saveVarInt(OS_inner, 2, it->second); // file_id
  }

  llvm::protobuf::saveVarInt(OS_inner, 3, Line);   // line
  llvm::protobuf::saveVarInt(OS_inner, 4, Column); // column

  OS_inner.str();

  return location_contents; // NRVO
}

static void trimSpaces(std::string::iterator &it,
                       std::string::iterator &it_end) {
  while (it < it_end) {
    if (*it == ' ')
      ++it;
    else if (*it_end == ' ')
      --it_end;
    else
      break;
  }
  ++it_end;
}

std::size_t
TemplightProtobufWriter::createDictionaryEntry(const std::string &NameOrig) {
  std::unordered_map<std::string, std::size_t>::iterator it_found =
      templateNameMap.find(NameOrig);
  if (it_found != templateNameMap.end())
    return it_found->second;

  // FIXME: Convert this code to being constructive of "Name", instead of
  // destructive (replacing sub-strings with '\0' characters).
  std::string Name = NameOrig;
  std::string::iterator it_open = Name.end();
  std::string::iterator it_colon_lo = Name.begin();
  int srch_state = 0;
  llvm::SmallVector<std::size_t, 8> markers;
  for (std::string::iterator it = Name.begin(); it != Name.end(); ++it) {
    switch (srch_state) {
    case 0:
      if (*it == '<') {
        // check for "operator<<", "operator<" and "operator<="
        llvm::StringRef test_str(Name.data(), it - Name.begin() + 1);
        if (test_str.ends_with("operator<")) {
          it_open = Name.end();
          srch_state = 0;
        } else {
          it_open = it;
          ++srch_state;
        }
      } else if ((*it == ':') && (it + 1 < Name.end()) && (*(it + 1) == ':')) {
        if (it_colon_lo < it) {
          markers.push_back(
              createDictionaryEntry(std::string(it_colon_lo, it)));
          std::size_t offset_lo = it_colon_lo - Name.begin();
          Name.replace(it_colon_lo, it, 1, '\0');
          it = Name.begin() + offset_lo + 2;
        } else {
          it += 1;
        }
        it_colon_lo = it + 1;
        it_open = Name.end();
      }
      break;
    case 1:
      if (*it == '<') {
        // check for "operator<<" and "operator<"
        llvm::StringRef test_str(Name.data(), it - Name.begin() + 1);
        if (test_str.ends_with("operator<<<")) {
          it_open = it;
          srch_state = 1;
        } else {
          ++srch_state;
        }
      } else if ((*it == ',') || (*it == '>')) {
        if (it_colon_lo < it_open) {
          std::size_t offset_end = it - it_open;
          std::size_t offset_lo = it_colon_lo - Name.begin();
          markers.push_back(
              createDictionaryEntry(std::string(it_colon_lo, it_open)));
          Name.replace(it_colon_lo, it_open, 1, '\0');
          it_open = Name.begin() + offset_lo + 1;
          it = it_open + offset_end;
          it_colon_lo = Name.end();
        }
        std::string::iterator it_lo = it_open + 1;
        std::string::iterator it_hi = it - 1;
        trimSpaces(it_lo, it_hi);
        // Create or find the marker entry:
        markers.push_back(createDictionaryEntry(std::string(it_lo, it_hi)));
        std::size_t offset_end = it - it_hi;
        std::size_t offset_lo = it_lo - Name.begin();
        Name.replace(it_lo, it_hi, 1, '\0');
        it = Name.begin() + offset_lo + 1 + offset_end;
        it_open = it;
        it_colon_lo = Name.end();
        if (*it == '>') {
          it_open = Name.end();
          srch_state = 0;
          it_colon_lo = it + 1;
        }
      }
      break;
    default:
      if (*it == '<') {
        ++srch_state;
      } else if (*it == '>') {
        --srch_state;
      }
      break;
    }
  }
  if (!markers.empty() && it_colon_lo != Name.end()) {
    markers.push_back(
        createDictionaryEntry(std::string(it_colon_lo, Name.end())));
    Name.replace(it_colon_lo, Name.end(), 1, '\0');
  }

  /*
  message DictionaryEntry {
    required string marked_name = 1;
    repeated uint32 marker_ids = 2;
  }
  */
  std::string dict_entry;
  llvm::raw_string_ostream OS_dict(dict_entry);
  llvm::protobuf::saveString(OS_dict, 1, Name); // marked_name
  for (llvm::SmallVector<std::size_t, 8>::iterator mk = markers.begin(),
                                                   mk_end = markers.end();
       mk != mk_end; ++mk) {
    llvm::protobuf::saveVarInt(OS_dict, 2, *mk); // marker_ids
  }
  OS_dict.str();

  std::size_t id = templateNameMap.size();
  templateNameMap[NameOrig] = id;

  std::string dict_field;
  {
    llvm::raw_string_ostream OS_outer(dict_field);
    // repeated DictionaryEntry names = 3;
    llvm::protobuf::saveString(OS_outer, 3, dict_entry);
  }
  appendToTrace(dict_field);

  return id;
}

std::string
TemplightProtobufWriter::printTemplateName(const std::string &Name) {

  /*
  message TemplateName {
    optional string name = 1;
    optional bytes compressed_name = 2;
  }
  */

  std::string tname_contents;
  llvm::raw_string_ostream OS_inner(tname_contents);

  switch (compressionMode) {
  case 1: { // zlib-compressed name:
    llvm::SmallVector<uint8_t, 32> CompressedBuffer;
    llvm::compression::zlib::compress(llvm::arrayRefFromStringRef(Name),
                                      CompressedBuffer);
    // optional bytes compressed_name = 2;
    llvm::protobuf::saveString(OS_inner, 2,
                               llvm::toStringRef(CompressedBuffer));
    break;
  }
    LLVM_FALLTHROUGH;
  case 0:
    // optional string name = 1;
    llvm::protobuf::saveString(OS_inner, 1, Name); // name
    break;
  case 2:
  default:
    // optional uint32 dict_id = 3;
    llvm::protobuf::saveVarInt(OS_inner, 3, createDictionaryEntry(Name));
    break;
  }

  OS_inner.str();

  return tname_contents; // NRVO
}

void TemplightProtobufWriter::printEntry(
    const PrintableTemplightEntryBegin &aEntry) {

  std::string entry_contents;
  {
    llvm::raw_string_ostream OS_inner(entry_contents);

    /*
  message Begin {
    required SynthesisKind kind = 1;
    required string name = 2;
    required SourceLocation location = 3;
    optional double time_stamp = 4;
    optional uint64 memory_usage = 5;
    optional SourceLocation template_origin = 6;
  }
    */

    llvm::protobuf::saveVarInt(OS_inner, 1, aEntry.SynthesisKind); // kind
    llvm::protobuf::saveString(OS_inner, 2,
                               printTemplateName(aEntry.Name)); // name
    llvm::protobuf::saveString(OS_inner, 3,
                               printEntryLocation(aEntry.FileName, aEntry.Line,
                                                  aEntry.Column)); // location
    llvm::protobuf::saveDouble(OS_inner, 4, aEntry.TimeStamp);     // time_stamp
    if (aEntry.MemoryUsage > 0)
      llvm::protobuf::saveVarInt(OS_inner, 5,
                                 aEntry.MemoryUsage); // memory_usage
    if (!aEntry.TempOri_FileName.empty())
      llvm::protobuf::saveString(
          OS_inner, 6,
          printEntryLocation(aEntry.TempOri_FileName, aEntry.TempOri_Line,
                             aEntry.TempOri_Column)); // template_origin
  }

  std::string oneof_contents;
  {
    llvm::raw_string_ostream OS_inner(oneof_contents);

    /*
  oneof begin_or_end {
    Begin begin = 1;
    End end = 2;
  }
     */

    llvm::protobuf::saveString(OS_inner, 1, entry_contents); // begin
  }

  std::string entry_field;
  {
    llvm::raw_string_ostream OS(entry_field);

    // repeated TemplightEntry entries = 2;
    llvm::protobuf::saveString(OS, 2, oneof_contents);
  }
  appendToTrace(entry_field);
}

void TemplightProtobufWriter::printEntry(
    const PrintableTemplightEntryEnd &aEntry) {

  std::string entry_contents;
  {
    llvm::raw_string_ostream OS_inner(entry_contents);

    /*
  message End {
    optional double time_stamp = 1;
    optional uint64 memory_usage = 2;
  }
    */

    llvm::protobuf::saveDouble(OS_inner, 1, aEntry.TimeStamp); // time_stamp
    if (aEntry.MemoryUsage > 0)
      llvm::protobuf::saveVarInt(OS_inner, 2,
                                 aEntry.MemoryUsage); // memory_usage
  }

  std::string oneof_contents;
  {
    llvm::raw_string_ostream OS_inner(oneof_contents);

    /*
  oneof begin_or_end {
    Begin begin = 1;
    End end = 2;
  }
     */

    llvm::protobuf::saveString(OS_inner, 2, entry_contents); // end
  }

  std::string entry_field;
  {
    llvm::raw_string_ostream OS(entry_field);

    // repeated TemplightEntry entries = 2;
    llvm::protobuf::saveString(OS, 2, oneof_contents);
  }
  appendToTrace(entry_field);
}

} // namespace clang

namespace clang {

std::string TemplightProtobufWriter::entryField(unsigned OneofField,
                                                const std::string &Contents) {
  std::string oneof_contents;
  {
    llvm::raw_string_ostream OS_inner(oneof_contents);
    llvm::protobuf::saveString(OS_inner, OneofField, Contents);
  }

  std::string entry_field;
  {
    llvm::raw_string_ostream OS(entry_field);

    // repeated TemplightEntry entries = 2;
    llvm::protobuf::saveString(OS, 2, oneof_contents);
  }
  return entry_field;
}

void TemplightProtobufWriter::appendEntry(unsigned OneofField,
                                          const std::string &Contents) {
  appendToTrace(entryField(OneofField, Contents));
}

std::string TemplightProtobufWriter::declReadContents(
    const PrintableTemplightEntryDeclRead &aEntry) {

  std::string entry_contents;
  {
    llvm::raw_string_ostream OS_inner(entry_contents);

    /*
  message DeclRead {
    required string kind = 1;
    required TemplateName name = 2;
    required string module_name = 3;
    required uint64 stack_depth_kb = 4;
  }
    */

    llvm::protobuf::saveString(OS_inner, 1, aEntry.Kind);
    llvm::protobuf::saveString(OS_inner, 2, printTemplateName(aEntry.Name));
    llvm::protobuf::saveString(OS_inner, 3, aEntry.ModuleName);
    llvm::protobuf::saveVarInt(OS_inner, 4, aEntry.StackDepthKB);
  }
  return entry_contents;
}

void TemplightProtobufWriter::printDeclRead(
    const PrintableTemplightEntryDeclRead &aEntry) {
  appendEntry(3, declReadContents(aEntry)); // decl_read
}

std::string TemplightProtobufWriter::unfinishedDeclReadText(
    const PrintableTemplightEntryDeclRead &aEntry) {
  // unfinished_decl_read
  return traceBytes(entryField(5, declReadContents(aEntry)));
}

void TemplightProtobufWriter::printMergedDecl(
    const PrintableTemplightEntryMergedDecl &aEntry) {

  std::string entry_contents;
  {
    llvm::raw_string_ostream OS_inner(entry_contents);

    /*
  message MergedDecl {
    required string kind = 1;
    required TemplateName name = 2;
    repeated string module_names = 3;
  }
    */

    llvm::protobuf::saveString(OS_inner, 1, aEntry.Kind);
    llvm::protobuf::saveString(OS_inner, 2, printTemplateName(aEntry.Name));
    for (const std::string &ModuleName : aEntry.ModuleNames)
      llvm::protobuf::saveString(OS_inner, 3, ModuleName);
  }
  appendEntry(4, entry_contents); // merged_decl
}

} // namespace clang

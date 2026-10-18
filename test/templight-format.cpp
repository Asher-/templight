// -format writes the trace in each accepted format.
// RUN: rm -rf %t && mkdir -p %t
// RUN: %templight_cc1 -fsyntax-only %s -Xtemplight -profiler \
// RUN:   -Xtemplight -format=protobuf -Xtemplight -output=%t/t.trace.pbf
// RUN: %python %S/Inputs/read-trace.py < %t/t.trace.pbf | FileCheck %s --check-prefix=PROTOBUF
// RUN: %templight_cc1 -fsyntax-only %s -Xtemplight -profiler \
// RUN:   -Xtemplight -format=yaml -Xtemplight -output=%t/t.trace.yaml
// RUN: FileCheck %s --check-prefix=YAML < %t/t.trace.yaml
// RUN: %templight_cc1 -fsyntax-only %s -Xtemplight -profiler \
// RUN:   -Xtemplight -format=xml -Xtemplight -output=%t/t.trace.xml
// RUN: FileCheck %s --check-prefix=XML < %t/t.trace.xml
// RUN: %templight_cc1 -fsyntax-only %s -Xtemplight -profiler \
// RUN:   -Xtemplight -format=text -Xtemplight -output=%t/t.trace.txt
// RUN: FileCheck %s --check-prefix=TEXT < %t/t.trace.txt
// RUN: %templight_cc1 -fsyntax-only %s -Xtemplight -profiler \
// RUN:   -Xtemplight -format=nestedxml -Xtemplight -output=%t/t.trace.nested.xml
// RUN: FileCheck %s --check-prefix=NESTEDXML < %t/t.trace.nested.xml
// RUN: %templight_cc1 -fsyntax-only %s -Xtemplight -profiler \
// RUN:   -Xtemplight -format=graphml -Xtemplight -output=%t/t.trace.graphml
// RUN: FileCheck %s --check-prefix=GRAPHML < %t/t.trace.graphml
// RUN: %templight_cc1 -fsyntax-only %s -Xtemplight -profiler \
// RUN:   -Xtemplight -format=graphviz -Xtemplight -output=%t/t.trace.dot
// RUN: FileCheck %s --check-prefix=GRAPHVIZ < %t/t.trace.dot

template <int N> struct Square {
  static const int value = N * N;
};
int Nine = Square<3>::value;

// PROTOBUF: header version=1 source={{.*}}templight-format.cpp
// PROTOBUF: begin kind=0 name=Square<3>
// PROTOBUF: end
// PROTOBUF-NOT: truncated

// YAML: ---
// YAML: - IsBegin: {{ *}}true
// YAML-NEXT: Kind: {{ *}}TemplateInstantiation
// YAML-NEXT: Name: {{ *}}'Square<3>'

// XML: <?xml version="1.0" standalone="yes"?>
// XML: <Trace>
// XML: <TemplateBegin>
// XML-NEXT: <Kind>TemplateInstantiation</Kind>
// XML-NEXT: <Context context = "Square&lt;3&gt;"/>
// XML: </Trace>

// TEXT: SourceFile = {{.*}}templight-format.cpp
// TEXT: TemplateBegin
// TEXT-NEXT: Kind = TemplateInstantiation
// TEXT-NEXT: Name = Square<3>

// NESTEDXML: <Trace>
// NESTEDXML: <Entry Kind="TemplateInstantiation" Name="Square&lt;3&gt;"
// NESTEDXML: </Trace>

// GRAPHML: <graphml xmlns="http://graphml.graphdrawing.org/xmlns"
// GRAPHML: Square&lt;3&gt;
// GRAPHML: </graphml>

// GRAPHVIZ: digraph Trace {
// GRAPHVIZ: n0 [label = "TemplateInstantiation\nSquare&lt;3&gt;
// GRAPHVIZ: }

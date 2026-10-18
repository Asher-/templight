// Under -safe-mode, a compilation that crashes leaves a trace that is complete
// in every format: each entry recorded before the crash, then what the format
// needs after it (closing tags, the end of the document or graph).
// RUN: rm -rf %t && mkdir -p %t
// RUN: not --crash %templight_cc1 -fsyntax-only %s -Xtemplight -profiler \
// RUN:   -Xtemplight -safe-mode -Xtemplight -format=yaml \
// RUN:   -Xtemplight -output=%t/t.trace.yaml
// RUN: FileCheck %s --check-prefix=YAML < %t/t.trace.yaml
// RUN: not --crash %templight_cc1 -fsyntax-only %s -Xtemplight -profiler \
// RUN:   -Xtemplight -safe-mode -Xtemplight -format=xml \
// RUN:   -Xtemplight -output=%t/t.trace.xml
// RUN: %python %S/Inputs/read-xml-trace.py < %t/t.trace.xml \
// RUN:   | FileCheck %s --check-prefix=XML
// RUN: not --crash %templight_cc1 -fsyntax-only %s -Xtemplight -profiler \
// RUN:   -Xtemplight -safe-mode -Xtemplight -format=text \
// RUN:   -Xtemplight -output=%t/t.trace.txt
// RUN: FileCheck %s --check-prefix=TEXT < %t/t.trace.txt
// RUN: not --crash %templight_cc1 -fsyntax-only %s -Xtemplight -profiler \
// RUN:   -Xtemplight -safe-mode -Xtemplight -format=nestedxml \
// RUN:   -Xtemplight -output=%t/t.trace.nested.xml
// RUN: %python %S/Inputs/read-xml-trace.py < %t/t.trace.nested.xml \
// RUN:   | FileCheck %s --check-prefix=NESTEDXML
// RUN: not --crash %templight_cc1 -fsyntax-only %s -Xtemplight -profiler \
// RUN:   -Xtemplight -safe-mode -Xtemplight -format=graphml \
// RUN:   -Xtemplight -output=%t/t.trace.graphml
// RUN: %python %S/Inputs/read-xml-trace.py < %t/t.trace.graphml \
// RUN:   | FileCheck %s --check-prefix=GRAPHML
// RUN: not --crash %templight_cc1 -fsyntax-only %s -Xtemplight -profiler \
// RUN:   -Xtemplight -safe-mode -Xtemplight -format=graphviz \
// RUN:   -Xtemplight -output=%t/t.trace.dot
// RUN: FileCheck %s --check-prefix=GRAPHVIZ < %t/t.trace.dot

template <int N> struct Factorial {
  static const int value = N * Factorial<N - 1>::value;
};
template <> struct Factorial<0> {
  static const int value = 1;
};

int Three = Factorial<3>::value;

#pragma clang __debug crash

// YAML: ---
// YAML: - IsBegin: {{ *}}true
// YAML-NEXT: Kind: {{ *}}TemplateInstantiation
// YAML-NEXT: Name: {{ *}}'Factorial<3>'
// YAML: Name: {{ *}}'Factorial<1>'
// YAML: - IsBegin: {{ *}}false
// YAML: {{^}}...{{$}}

// XML: root=Trace
// XML-NEXT: context Factorial<3>
// XML: context Factorial<1>

// TEXT: SourceFile = {{.*}}templight-safe-mode-crash-formats.cpp
// TEXT: TemplateBegin
// TEXT-NEXT: Kind = TemplateInstantiation
// TEXT-NEXT: Name = Factorial<3>
// TEXT: Name = Factorial<1>
// TEXT: TemplateEnd

// NESTEDXML: root=Trace
// NESTEDXML-NEXT: entry Factorial<3>
// NESTEDXML-NEXT: exit
// NESTEDXML-NEXT: entry Factorial<3>
// NESTEDXML-NEXT: entry Factorial<2>
// NESTEDXML: entry Factorial<1>
// NESTEDXML: entry Factorial<0>
// NESTEDXML-NEXT: exit
// NESTEDXML-NEXT: exit

// GRAPHML: root=graphml
// GRAPHML-NEXT: node tu "{{.*}}templight-safe-mode-crash-formats.cpp"
// GRAPHML-NEXT: node n0 "Factorial<3>"
// GRAPHML-NEXT: edge tu->n0
// GRAPHML-NEXT: node n1 "Factorial<3>"
// GRAPHML-NEXT: node n2 "Factorial<2>"
// GRAPHML: edge n1->n2
// GRAPHML: edge tu->n1

// GRAPHVIZ: digraph Trace {
// GRAPHVIZ-NEXT: n0 [label = "TemplateInstantiation\nFactorial&lt;3&gt;\n{{.*}}" ];
// GRAPHVIZ-NEXT: n0 [label = "TemplateInstantiation\nFactorial&lt;3&gt;\n{{.*}}Time: {{.*}} seconds
// GRAPHVIZ-NEXT: n1 [label = "TemplateInstantiation\nFactorial&lt;3&gt;\n
// GRAPHVIZ-NEXT: n2 [label = "TemplateInstantiation\nFactorial&lt;2&gt;\n
// GRAPHVIZ-NEXT: n1 -> n2;
// GRAPHVIZ: {{^}}}{{$}}

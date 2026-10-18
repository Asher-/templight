// Under -safe-mode, a compilation that overflows the main thread's stack still
// leaves a complete trace: the crash handlers run on the alternate signal stack
// that templight installs, and write what each format needs after the last
// entry. The shell lowers the stack limit, which lit's ulimit cannot do on
// macOS.
// UNSUPPORTED: system-windows
// RUN: rm -rf %t && mkdir -p %t
// RUN: not --crash sh -c 'ulimit -s 2048 && exec %templight_cc1 -fsyntax-only \
// RUN:   -ftemplate-depth=200000 %s -Xtemplight -profiler \
// RUN:   -Xtemplight -safe-mode -Xtemplight -format=xml \
// RUN:   -Xtemplight -output=%t/t.trace.xml'
// RUN: %python %S/Inputs/read-xml-trace.py < %t/t.trace.xml \
// RUN:   | FileCheck %s --check-prefix=XML
// RUN: not --crash sh -c 'ulimit -s 2048 && exec %templight_cc1 -fsyntax-only \
// RUN:   -ftemplate-depth=200000 %s -Xtemplight -profiler \
// RUN:   -Xtemplight -safe-mode -Xtemplight -format=nestedxml \
// RUN:   -Xtemplight -output=%t/t.trace.nested.xml'
// RUN: %python %S/Inputs/read-xml-trace.py < %t/t.trace.nested.xml \
// RUN:   | FileCheck %s --check-prefix=NESTEDXML
// RUN: not --crash sh -c 'ulimit -s 2048 && exec %templight_cc1 -fsyntax-only \
// RUN:   -ftemplate-depth=200000 %s -Xtemplight -profiler \
// RUN:   -Xtemplight -safe-mode -Xtemplight -format=graphml \
// RUN:   -Xtemplight -output=%t/t.trace.graphml'
// RUN: %python %S/Inputs/read-xml-trace.py < %t/t.trace.graphml \
// RUN:   | FileCheck %s --check-prefix=GRAPHML
// RUN: not --crash sh -c 'ulimit -s 2048 && exec %templight_cc1 -fsyntax-only \
// RUN:   -ftemplate-depth=200000 %s -Xtemplight -profiler \
// RUN:   -Xtemplight -safe-mode -Xtemplight -format=graphviz \
// RUN:   -Xtemplight -output=%t/t.trace.dot'
// RUN: FileCheck %s --check-prefix=GRAPHVIZ < %t/t.trace.dot
// RUN: not --crash sh -c 'ulimit -s 2048 && exec %templight_cc1 -fsyntax-only \
// RUN:   -ftemplate-depth=200000 %s -Xtemplight -profiler \
// RUN:   -Xtemplight -safe-mode -Xtemplight -format=yaml \
// RUN:   -Xtemplight -output=%t/t.trace.yaml'
// RUN: FileCheck %s --check-prefix=YAML < %t/t.trace.yaml

template <int N> struct R { static const int value = R<N - 1>::value + 1; };
template <> struct R<0> { static const int value = 0; };
int X = R<100000>::value;

// XML: root=Trace
// XML-NEXT: context R<100000>
// XML: context R<99990>

// NESTEDXML: root=Trace
// NESTEDXML-NEXT: entry R<100000>
// NESTEDXML: entry R<99990>

// GRAPHML: root=graphml
// GRAPHML-NEXT: node tu "{{.*}}templight-safe-mode-overflow.cpp"
// GRAPHML-NEXT: node n0 "R<100000>"
// GRAPHML: node {{n[0-9]+}} "R<99990>"
// GRAPHML: edge tu->n1

// GRAPHVIZ: digraph Trace {
// GRAPHVIZ-NEXT: n0 [label = "TemplateInstantiation\nR&lt;100000&gt;\n
// GRAPHVIZ: R&lt;99990&gt;
// GRAPHVIZ: {{^}}}{{$}}

// YAML: ---
// YAML-NEXT: - IsBegin: {{ *}}true
// YAML-NEXT: Kind: {{ *}}TemplateInstantiation
// YAML-NEXT: Name: {{ *}}'R<100000>'
// YAML: Name: {{ *}}'R<99990>'
// YAML: {{^}}...{{$}}

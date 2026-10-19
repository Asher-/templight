// Under -safe-mode, a compilation whose stack overflows inside clang's module
// reader, on a stack clang started for the read, leaves a complete trace.
// Reading U5000 recurses through 5000 typedefs, more than one stack holds, so
// clang goes on with the read on a thread of its own (llvm::runOnNewStack);
// there U0's read reaches P, whose 100000 pointer types overflow that thread's
// stack before the thread writes any entry. The crash handler can only run on
// that thread's alternate signal stack, which the thread gets as it starts.
// UNSUPPORTED: system-windows
// RUN: rm -rf %t && mkdir -p %t
// RUN: %python %S/Inputs/make-deep-module.py %t/deep 5000 100000
// RUN: not --crash %templight_cc1 -std=c++17 -fmodules \
// RUN:   -fimplicit-module-maps -fmodules-cache-path=%t/cache -I %t/deep \
// RUN:   -fsyntax-only %s -Xtemplight -profiler -Xtemplight -deserialization \
// RUN:   -Xtemplight -safe-mode -Xtemplight -format=xml \
// RUN:   -Xtemplight -output=%t/t.trace.xml
// RUN: %python %S/Inputs/read-xml-trace.py < %t/t.trace.xml \
// RUN:   | FileCheck %s --check-prefix=XML-PARSE
// RUN: FileCheck %s --check-prefix=XML < %t/t.trace.xml
// RUN: not --crash %templight_cc1 -std=c++17 -fmodules \
// RUN:   -fimplicit-module-maps -fmodules-cache-path=%t/cache -I %t/deep \
// RUN:   -fsyntax-only %s -Xtemplight -profiler -Xtemplight -deserialization \
// RUN:   -Xtemplight -safe-mode -Xtemplight -format=nestedxml \
// RUN:   -Xtemplight -output=%t/t.trace.nested.xml
// RUN: %python %S/Inputs/read-xml-trace.py < %t/t.trace.nested.xml \
// RUN:   | FileCheck %s --check-prefix=XML-PARSE
// RUN: not --crash %templight_cc1 -std=c++17 -fmodules \
// RUN:   -fimplicit-module-maps -fmodules-cache-path=%t/cache -I %t/deep \
// RUN:   -fsyntax-only %s -Xtemplight -profiler -Xtemplight -deserialization \
// RUN:   -Xtemplight -safe-mode -Xtemplight -format=graphml \
// RUN:   -Xtemplight -output=%t/t.trace.graphml
// RUN: %python %S/Inputs/read-xml-trace.py < %t/t.trace.graphml \
// RUN:   | FileCheck %s --check-prefix=GRAPHML-PARSE
// RUN: not --crash %templight_cc1 -std=c++17 -fmodules \
// RUN:   -fimplicit-module-maps -fmodules-cache-path=%t/cache -I %t/deep \
// RUN:   -fsyntax-only %s -Xtemplight -profiler -Xtemplight -deserialization \
// RUN:   -Xtemplight -safe-mode -Xtemplight -format=graphviz \
// RUN:   -Xtemplight -output=%t/t.trace.dot
// RUN: FileCheck %s --check-prefix=GRAPHVIZ < %t/t.trace.dot
// RUN: not --crash %templight_cc1 -std=c++17 -fmodules \
// RUN:   -fimplicit-module-maps -fmodules-cache-path=%t/cache -I %t/deep \
// RUN:   -fsyntax-only %s -Xtemplight -profiler -Xtemplight -deserialization \
// RUN:   -Xtemplight -safe-mode -Xtemplight -format=yaml \
// RUN:   -Xtemplight -output=%t/t.trace.yaml
// RUN: FileCheck %s --check-prefix=YAML < %t/t.trace.yaml

#include "deep.h"

T100 A;
U5000 C;

// XML-PARSE: root=Trace

// XML: <Name>T100</Name>
// XML-NEXT: <Module>Deep</Module>
// XML: </Trace>

// GRAPHML-PARSE: root=graphml
// GRAPHML-PARSE: node {{r[0-9]+}} "T100"

// GRAPHVIZ: digraph Trace {
// GRAPHVIZ: T100\nModule: Deep
// GRAPHVIZ: {{^}}}{{$}}

// YAML: Name: {{ *}}T100
// YAML-NEXT: Module: {{ *}}Deep
// YAML: {{^}}...{{$}}

// Under -safe-mode, a compilation whose stack overflows inside clang's module
// reader leaves a complete trace that holds each declaration read before the
// overflow. Reading P recurses through 100000 pointer types. A declaration's
// entry is written when its read completes; P, whose read the overflow cuts
// short, is named as an unfinished read, with the stack depth at its start.
// UNSUPPORTED: system-windows
// RUN: rm -rf %t && mkdir -p %t
// RUN: %python %S/Inputs/make-deep-module.py %t/deep 100 100000
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
// RUN: FileCheck %s --check-prefix=NESTEDXML < %t/t.trace.nested.xml

#include "deep.h"

T100 A;
P B;

// XML-PARSE: root=Trace

// XML: <Name>T100</Name>
// XML-NEXT: <Module>Deep</Module>
// XML-NEXT: <StackDepth kb = "{{[0-9]+}}"/>
// XML-NOT: <Name>P</Name>
// XML: <UnfinishedDeclRead>
// XML-NEXT: <Kind>Typedef</Kind>
// XML-NEXT: <Name>P</Name>
// XML-NEXT: <Module>Deep</Module>
// XML-NEXT: <StackDepth kb = "{{[0-9]+}}"/>
// XML-NEXT: </UnfinishedDeclRead>
// XML: </Trace>

// NESTEDXML: Name="T100" Module="Deep" StackDepthKB="{{[0-9]+}}"/>
// NESTEDXML-NOT: Name="P"
// NESTEDXML: <UnfinishedDeclRead Kind="Typedef" Name="P" Module="Deep"
// StackDepthKB="{{[0-9]+}}"/> NESTEDXML: </Trace>

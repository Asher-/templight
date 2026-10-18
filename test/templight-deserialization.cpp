// -deserialization writes one entry per declaration read from a module file,
// and at the end of the translation unit one entry for each declaration read
// from two or more module files. DeserA and DeserB each hold their own copy of
// Chain<3>, so its trace names both of their module files.
// RUN: rm -rf %t && mkdir -p %t
// RUN: %templight_cc1 -std=c++17 -fmodules -fimplicit-module-maps \
// RUN:   -fmodules-cache-path=%t/cache -I %S/Inputs/deserialization \
// RUN:   -fsyntax-only %s -Xtemplight -profiler -Xtemplight -deserialization \
// RUN:   -Xtemplight -output=%t/t.trace.pbf
// RUN: %python %S/Inputs/read-trace.py < %t/t.trace.pbf | FileCheck %s
// RUN: %templight_cc1 -std=c++17 -fmodules -fimplicit-module-maps \
// RUN:   -fmodules-cache-path=%t/cache -I %S/Inputs/deserialization \
// RUN:   -fsyntax-only %s -Xtemplight -profiler -Xtemplight -deserialization \
// RUN:   -Xtemplight -format=xml -Xtemplight -output=%t/t.trace.xml
// RUN: %python %S/Inputs/read-xml-trace.py < %t/t.trace.xml \
// RUN:   | FileCheck %s --check-prefix=XML-PARSE
// RUN: FileCheck %s --check-prefix=XML < %t/t.trace.xml
// RUN: %templight_cc1 -std=c++17 -fmodules -fimplicit-module-maps \
// RUN:   -fmodules-cache-path=%t/cache -I %S/Inputs/deserialization \
// RUN:   -fsyntax-only %s -Xtemplight -profiler -Xtemplight -deserialization \
// RUN:   -Xtemplight -format=text -Xtemplight -output=%t/t.trace.txt
// RUN: FileCheck %s --check-prefix=TEXT < %t/t.trace.txt
// RUN: %templight_cc1 -std=c++17 -fmodules -fimplicit-module-maps \
// RUN:   -fmodules-cache-path=%t/cache -I %S/Inputs/deserialization \
// RUN:   -fsyntax-only %s -Xtemplight -profiler -Xtemplight -deserialization \
// RUN:   -Xtemplight -format=yaml -Xtemplight -output=%t/t.trace.yaml
// RUN: FileCheck %s --check-prefix=YAML < %t/t.trace.yaml
// RUN: %templight_cc1 -std=c++17 -fmodules -fimplicit-module-maps \
// RUN:   -fmodules-cache-path=%t/cache -I %S/Inputs/deserialization \
// RUN:   -fsyntax-only %s -Xtemplight -profiler -Xtemplight -deserialization \
// RUN:   -Xtemplight -format=nestedxml \
// RUN:   -Xtemplight -output=%t/t.trace.nested.xml
// RUN: %python %S/Inputs/read-xml-trace.py < %t/t.trace.nested.xml \
// RUN:   | FileCheck %s --check-prefix=XML-PARSE
// RUN: FileCheck %s --check-prefix=NESTEDXML < %t/t.trace.nested.xml
// RUN: %templight_cc1 -std=c++17 -fmodules -fimplicit-module-maps \
// RUN:   -fmodules-cache-path=%t/cache -I %S/Inputs/deserialization \
// RUN:   -fsyntax-only %s -Xtemplight -profiler -Xtemplight -deserialization \
// RUN:   -Xtemplight -safe-mode -Xtemplight -format=nestedxml \
// RUN:   -Xtemplight -output=%t/t.trace.nested.safe.xml
// RUN: %python %S/Inputs/read-xml-trace.py < %t/t.trace.nested.safe.xml \
// RUN:   | FileCheck %s --check-prefix=XML-PARSE
// RUN: FileCheck %s --check-prefix=NESTEDXML < %t/t.trace.nested.safe.xml

#include "a.h"
#include "b.h"

int Sum = deserA() + deserB();

// CHECK: header version={{[0-9]+}} source={{.*}}templight-deserialization.cpp
// CHECK-DAG: decl-read kind=ClassTemplateSpecialization name=Chain<3> module=DeserA stack-kb={{[0-9]+}}
// CHECK-DAG: decl-read kind=ClassTemplateSpecialization name=Chain<3> module=DeserB stack-kb={{[0-9]+}}
// CHECK: merged-decl kind=ClassTemplateSpecialization name=Chain<3> modules=DeserA,DeserB
// CHECK: entries-done

// XML-PARSE: root=Trace

// XML: <Kind>ClassTemplateSpecialization</Kind>
// XML-NEXT: <Name>Chain&lt;3&gt;</Name>
// XML-NEXT: <Module>DeserA</Module>
// XML-NEXT: <StackDepth kb = "{{[0-9]+}}"/>
// XML: <Kind>ClassTemplateSpecialization</Kind>
// XML-NEXT: <Name>Chain&lt;3&gt;</Name>
// XML-NEXT: <Module>DeserB</Module>
// XML: <MergedDecl>
// XML: <Kind>ClassTemplateSpecialization</Kind>
// XML-NEXT: <Name>Chain&lt;3&gt;</Name>
// XML-NEXT: <Module>DeserA</Module>
// XML-NEXT: <Module>DeserB</Module>
// XML-NEXT: </MergedDecl>
// XML: </Trace>

// TEXT: DeclRead
// TEXT: Name = Chain<3>{{$}}
// TEXT-NEXT: Module = DeserA
// TEXT-NEXT: StackDepthKB = {{[0-9]+}}
// TEXT: Name = Chain<3>{{$}}
// TEXT-NEXT: Module = DeserB
// TEXT: MergedDecl
// TEXT: Kind = ClassTemplateSpecialization
// TEXT-NEXT: Name = Chain<3>{{$}}
// TEXT-NEXT: Module = DeserA
// TEXT-NEXT: Module = DeserB

// YAML: - DeclRead: {{ *}}ClassTemplateSpecialization
// YAML-NEXT: Name: {{ *}}'Chain<3>'
// YAML-NEXT: Module: {{ *}}DeserA
// YAML-NEXT: StackDepthKB: {{ *}}{{[0-9]+}}
// YAML: - DeclRead: {{ *}}ClassTemplateSpecialization
// YAML-NEXT: Name: {{ *}}'Chain<3>'
// YAML-NEXT: Module: {{ *}}DeserB
// YAML: - MergedDecl: {{ *}}ClassTemplateSpecialization
// YAML-NEXT: Name: {{ *}}'Chain<3>'
// YAML-NEXT: Modules:
// YAML-NEXT: - DeserA
// YAML-NEXT: - DeserB
// YAML: {{^}}...{{$}}

// NESTEDXML: Name="Chain&lt;3&gt;" Module="DeserA" StackDepthKB="{{[0-9]+}}"/>
// NESTEDXML: Name="Chain&lt;3&gt;" Module="DeserB" StackDepthKB="{{[0-9]+}}"/>
// NESTEDXML: <MergedDecl Kind="ClassTemplateSpecialization" Name="Chain&lt;3&gt;">
// NESTEDXML-NEXT: <Module Name="DeserA"/>
// NESTEDXML-NEXT: <Module Name="DeserB"/>
// NESTEDXML-NEXT: </MergedDecl>
// NESTEDXML: </Trace>

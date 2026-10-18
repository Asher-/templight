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

#include "a.h"
#include "b.h"

int Sum = deserA() + deserB();

// CHECK: header version={{[0-9]+}} source={{.*}}templight-deserialization.cpp
// CHECK-DAG: decl-read kind=ClassTemplateSpecialization name=Chain<3> module=DeserA stack-kb={{[0-9]+}}
// CHECK-DAG: decl-read kind=ClassTemplateSpecialization name=Chain<3> module=DeserB stack-kb={{[0-9]+}}
// CHECK: merged-decl kind=ClassTemplateSpecialization name=Chain<3> modules=DeserA,DeserB
// CHECK: entries-done

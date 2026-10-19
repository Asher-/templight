// -deserialization names a declaration read from a precompiled header, which
// has no module name, by the header's file.
// RUN: rm -rf %t && mkdir -p %t
// RUN: %templight_cc1 -std=c++17 -x c++-header -emit-pch \
// RUN:   %S/Inputs/deserialization/chain.h -o %t/chain.pch
// RUN: %templight_cc1 -std=c++17 -include-pch %t/chain.pch -fsyntax-only %s \
// RUN:   -Xtemplight -profiler -Xtemplight -deserialization \
// RUN:   -Xtemplight -output=%t/t.trace.pbf
// RUN: templight-read-trace < %t/t.trace.pbf \
// RUN:   | FileCheck %s --implicit-check-not='module= stack-kb'

int Three = Chain<3>::value;

// CHECK: decl-read kind=ClassTemplate name=Chain module={{.*}}chain.pch stack-kb={{[0-9]+}}
// CHECK: decl-read kind=ClassTemplateSpecialization name=Chain<0> module={{.*}}chain.pch
// CHECK: entries-done

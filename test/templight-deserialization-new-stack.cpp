// -deserialization measures each read's stack depth below the base of the
// stack the read runs on. Reading T5000 recurses through 5000 typedefs, more
// than one stack holds, so clang warns and goes on with the read on a stack of
// its own (llvm::runOnNewStack). The reads that finish there are written first
// and count up from that stack's base; the rest are back on the first stack.
// RUN: rm -rf %t && mkdir -p %t
// RUN: %python %S/Inputs/make-deep-module.py %t/deep 5000 0
// RUN: %templight_cc1 -std=c++17 -fmodules -fimplicit-module-maps \
// RUN:   -fmodules-cache-path=%t/cache -I %t/deep -fsyntax-only %s \
// RUN:   -Xtemplight -profiler -Xtemplight -deserialization \
// RUN:   -Xtemplight -format=text -Xtemplight -output=%t/t.trace.txt 2>&1 \
// RUN:   | FileCheck %s --check-prefix=WARN
// RUN: %python %S/Inputs/check-stack-depths.py 8192 < %t/t.trace.txt \
// RUN:   | FileCheck %s

#include "deep.h"

T5000 X;

// WARN: warning: stack nearly exhausted

// CHECK: reads={{[0-9]+}} new-stack-deepest-kb={{[0-9]+}} ok

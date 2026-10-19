// Under -safe-mode, a compilation that ends in SIGABRT leaves a complete trace:
// a fatal LLVM error aborts the compiler after the instantiations.
// RUN: rm -rf %t && mkdir -p %t
// RUN: not --crash %templight_cc1 -fsyntax-only %s \
// RUN:   -Xtemplight -profiler -Xtemplight -safe-mode -Xtemplight -format=xml \
// RUN:   -Xtemplight -output=%t/t.trace.xml 2>&1 | FileCheck %s --check-prefix=ABORT
// RUN: %python %S/Inputs/read-xml-trace.py < %t/t.trace.xml \
// RUN:   | FileCheck %s --check-prefix=XML
// RUN: not --crash %templight_cc1 -fsyntax-only %s \
// RUN:   -Xtemplight -profiler -Xtemplight -safe-mode -Xtemplight -format=nestedxml \
// RUN:   -Xtemplight -output=%t/t.trace.nested.xml
// RUN: %python %S/Inputs/read-xml-trace.py < %t/t.trace.nested.xml \
// RUN:   | FileCheck %s --check-prefix=NESTEDXML

template <int N> struct Factorial {
  static const int value = N * Factorial<N - 1>::value;
};
template <> struct Factorial<0> {
  static const int value = 1;
};

int Three = Factorial<3>::value;

#pragma clang __debug llvm_fatal_error

// ABORT: LLVM ERROR: #pragma clang __debug llvm_fatal_error

// XML: root=Trace
// XML-NEXT: context Factorial<3>
// XML: context Factorial<1>

// NESTEDXML: root=Trace
// NESTEDXML-NEXT: entry Factorial<3>
// NESTEDXML: entry Factorial<1>

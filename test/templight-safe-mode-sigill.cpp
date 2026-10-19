// Under -safe-mode, a compilation that ends in SIGILL leaves a complete trace:
// the plugin's pragma executes an illegal instruction after the instantiations.
// REQUIRES: illegal-instruction-plugin
// RUN: rm -rf %t && mkdir -p %t
// RUN: sh -c '%templight_cc1 -load %illegal_instruction_plugin -fsyntax-only \
// RUN:   %s -Xtemplight -profiler -Xtemplight -safe-mode -Xtemplight -format=xml \
// RUN:   -Xtemplight -output=%t/t.trace.xml 2>/dev/null; echo "exit=$?"' \
// RUN:   | FileCheck %s --check-prefix=SIGILL
// RUN: %python %S/Inputs/read-xml-trace.py < %t/t.trace.xml \
// RUN:   | FileCheck %s --check-prefix=XML
// RUN: sh -c '%templight_cc1 -load %illegal_instruction_plugin -fsyntax-only \
// RUN:   %s -Xtemplight -profiler -Xtemplight -safe-mode \
// RUN:   -Xtemplight -format=nestedxml -Xtemplight -output=%t/t.trace.nested.xml \
// RUN:   2>/dev/null; echo "exit=$?"' | FileCheck %s --check-prefix=SIGILL
// RUN: %python %S/Inputs/read-xml-trace.py < %t/t.trace.nested.xml \
// RUN:   | FileCheck %s --check-prefix=NESTEDXML
// RUN: sh -c '%templight_cc1 -load %illegal_instruction_plugin -fsyntax-only \
// RUN:   %s -Xtemplight -profiler -Xtemplight -safe-mode \
// RUN:   -Xtemplight -output=%t/t.trace.pbf 2>/dev/null; echo "exit=$?"' \
// RUN:   | FileCheck %s --check-prefix=SIGILL
// RUN: templight-read-trace < %t/t.trace.pbf | FileCheck %s --check-prefix=PBF

template <int N> struct Factorial {
  static const int value = N * Factorial<N - 1>::value;
};
template <> struct Factorial<0> {
  static const int value = 1;
};

int Three = Factorial<3>::value;

#pragma templight_illegal_instruction

// SIGILL: exit=132

// XML: root=Trace
// XML-NEXT: context Factorial<3>
// XML: context Factorial<1>

// NESTEDXML: root=Trace
// NESTEDXML-NEXT: entry Factorial<3>
// NESTEDXML: entry Factorial<1>

// PBF: header version=1 source={{.*}}templight-safe-mode-sigill.cpp
// PBF: begin kind={{[0-9]+}} name=Factorial<3>
// PBF: begin kind={{[0-9]+}} name=Factorial<1>
// PBF: entries-done

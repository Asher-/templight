// -format=graphviz writes the trace in the graphviz format.
// RUN: rm -f %t.trace.dot
// RUN: %templight_cc1 -fsyntax-only %s -Xtemplight -profiler \
// RUN:   -Xtemplight -format=graphviz -Xtemplight -output=%t.trace.dot
// RUN: FileCheck %s < %t.trace.dot

template <int N> struct Square {
  static const int value = N * N;
};
int Nine = Square<3>::value;

// CHECK: digraph Trace {
// CHECK: n0 [label = "TemplateInstantiation\nSquare&lt;3&gt;
// CHECK: }

// -format=graphml writes the trace in the graphml format.
// RUN: rm -f %t.trace.graphml
// RUN: %templight_cc1 -fsyntax-only %s -Xtemplight -profiler \
// RUN:   -Xtemplight -format=graphml -Xtemplight -output=%t.trace.graphml
// RUN: FileCheck %s < %t.trace.graphml

template <int N> struct Square {
  static const int value = N * N;
};
int Nine = Square<3>::value;

// CHECK: <graphml xmlns="http://graphml.graphdrawing.org/xmlns"
// CHECK: Square&lt;3&gt;
// CHECK: </graphml>

// -format=nestedxml writes the trace in the nestedxml format.
// RUN: rm -f %t.trace.nested.xml
// RUN: %templight_cc1 -fsyntax-only %s -Xtemplight -profiler \
// RUN:   -Xtemplight -format=nestedxml -Xtemplight -output=%t.trace.nested.xml
// RUN: FileCheck %s < %t.trace.nested.xml

template <int N> struct Square {
  static const int value = N * N;
};
int Nine = Square<3>::value;

// CHECK: <Trace>
// CHECK: <Entry Kind="TemplateInstantiation" Name="Square&lt;3&gt;"
// CHECK: </Trace>

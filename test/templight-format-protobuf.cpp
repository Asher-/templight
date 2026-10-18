// -format=protobuf (the default) writes the trace in the protobuf format.
// RUN: rm -f %t.trace.pbf
// RUN: %templight_cc1 -fsyntax-only %s -Xtemplight -profiler \
// RUN:   -Xtemplight -format=protobuf -Xtemplight -output=%t.trace.pbf
// RUN: %python %S/Inputs/read-trace.py < %t.trace.pbf | FileCheck %s

template <int N> struct Square {
  static const int value = N * N;
};
int Nine = Square<3>::value;

// CHECK: header version=1 source={{.*}}templight-format-protobuf.cpp
// CHECK: begin kind=0 name=Square<3>
// CHECK: end
// CHECK-NOT: truncated

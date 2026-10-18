// -format=yaml writes the trace in the yaml format.
// RUN: rm -f %t.trace.yaml
// RUN: %templight_cc1 -fsyntax-only %s -Xtemplight -profiler \
// RUN:   -Xtemplight -format=yaml -Xtemplight -output=%t.trace.yaml
// RUN: FileCheck %s < %t.trace.yaml

template <int N> struct Square {
  static const int value = N * N;
};
int Nine = Square<3>::value;

// CHECK: ---
// CHECK: - IsBegin: {{ *}}true
// CHECK-NEXT: Kind: {{ *}}TemplateInstantiation
// CHECK-NEXT: Name: {{ *}}'Square<3>'

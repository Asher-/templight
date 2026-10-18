// -format=text writes the trace in the text format.
// RUN: rm -f %t.trace.txt
// RUN: %templight_cc1 -fsyntax-only %s -Xtemplight -profiler \
// RUN:   -Xtemplight -format=text -Xtemplight -output=%t.trace.txt
// RUN: FileCheck %s < %t.trace.txt

template <int N> struct Square {
  static const int value = N * N;
};
int Nine = Square<3>::value;

// CHECK: SourceFile = {{.*}}templight-format-text.cpp
// CHECK: TemplateBegin
// CHECK-NEXT: Kind = TemplateInstantiation
// CHECK-NEXT: Name = Square<3>

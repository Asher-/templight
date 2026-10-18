// -format=xml writes the trace in the xml format.
// RUN: rm -f %t.trace.xml
// RUN: %templight_cc1 -fsyntax-only %s -Xtemplight -profiler \
// RUN:   -Xtemplight -format=xml -Xtemplight -output=%t.trace.xml
// RUN: FileCheck %s < %t.trace.xml

template <int N> struct Square {
  static const int value = N * N;
};
int Nine = Square<3>::value;

// CHECK: <?xml version="1.0" standalone="yes"?>
// CHECK: <Trace>
// CHECK: <TemplateBegin>
// CHECK-NEXT: <Kind>TemplateInstantiation</Kind>
// CHECK-NEXT: <Context context = "Square&lt;3&gt;"/>
// CHECK: </Trace>

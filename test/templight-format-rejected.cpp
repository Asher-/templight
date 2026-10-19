// An unknown -format name is refused with the accepted names, and no trace
// file is written, whether templight runs as -cc1 or as the driver.
// RUN: rm -f %t.trace.pbf %t.trace.json %t.driver.trace.json
// RUN: not %templight_cc1 -fsyntax-only %s -Xtemplight -profiler \
// RUN:   -Xtemplight -format=json -Xtemplight -output=%t.trace.json 2>&1 \
// RUN:   | FileCheck %s
// RUN: not ls %t.trace.json
// RUN: not templight -fsyntax-only %s -Xtemplight -profiler \
// RUN:   -Xtemplight -format=json -Xtemplight -output=%t.driver.trace.json 2>&1 \
// RUN:   | FileCheck %s
// RUN: not ls %t.driver.trace.json

// CHECK: error: [Templight] unknown trace format 'json'; accepted formats: protobuf, yaml, xml, text, nestedxml, graphml, graphviz

template <int N> struct Square {
  static const int value = N * N;
};
int Nine = Square<3>::value;

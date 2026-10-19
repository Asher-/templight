// A compilation that crashes leaves a readable trace under -safe-mode: every
// instantiation recorded before the crash is in the file, and
// utils/ProtobufReader (templight-read-trace) reads it.
// RUN: rm -f %t.trace.pbf
// RUN: not --crash %templight_cc1 -fsyntax-only %s -Xtemplight -profiler \
// RUN:   -Xtemplight -safe-mode -Xtemplight -output=%t.trace.pbf
// RUN: %python %S/Inputs/read-trace.py < %t.trace.pbf | FileCheck %s
// RUN: templight-read-trace < %t.trace.pbf | FileCheck %s

// Written to a pipe, the trace cannot be patched in place, so each entry is a
// trace record of its own; the reader reads them on as one trace.
// RUN: not --crash %templight_cc1 -fsyntax-only %s -Xtemplight -profiler \
// RUN:   -Xtemplight -safe-mode -Xtemplight -stdout \
// RUN:   | %python %S/Inputs/read-trace.py | FileCheck %s
// RUN: not --crash %templight_cc1 -fsyntax-only %s -Xtemplight -profiler \
// RUN:   -Xtemplight -safe-mode -Xtemplight -stdout \
// RUN:   | templight-read-trace | FileCheck %s

// Without -safe-mode the trace is held in memory until the end of the
// translation unit, so the crash leaves no trace entries.
// RUN: rm -f %t.buffered.trace.pbf
// RUN: not --crash %templight_cc1 -fsyntax-only %s -Xtemplight -profiler \
// RUN:   -Xtemplight -output=%t.buffered.trace.pbf
// RUN: %python %S/Inputs/read-trace.py < %t.buffered.trace.pbf \
// RUN:   | FileCheck %s --check-prefix=BUFFERED
// RUN: templight-read-trace < %t.buffered.trace.pbf \
// RUN:   | FileCheck %s --check-prefix=BUFFERED

template <int N> struct Factorial {
  static const int value = N * Factorial<N - 1>::value;
};
template <> struct Factorial<0> {
  static const int value = 1;
};

int Three = Factorial<3>::value;

#pragma clang __debug crash

// CHECK: header version=1 source={{.*}}templight-safe-mode-crash.cpp
// CHECK: begin kind={{[0-9]+}} name=Factorial<3>
// CHECK: begin kind={{[0-9]+}} name=Factorial<2>
// CHECK: begin kind={{[0-9]+}} name=Factorial<1>
// CHECK: end
// CHECK-NOT: truncated
// CHECK: entries-done

// BUFFERED-NOT: begin
// BUFFERED: entries-done

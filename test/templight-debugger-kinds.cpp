// The debugger names each entry's kind of code synthesis from clang's
// SynthesisKind, kinds past the first nine included (memoization is kind 23).
// RUN: printf 's\ns\ns\ns\ns\ns\ns\ns\nq\n' \
// RUN:   | %templight_cc1 -fsyntax-only %s -Xtemplight -debugger | FileCheck %s

template <class T> struct A { T v; };
struct S { A<int> a; };
void f() {
  S s;
  S t = s;
  (void)t;
}

// CHECK: Entering template instantiation of A<int>
// CHECK: Entering memoization of A<int>
// CHECK: Entering memoization of S

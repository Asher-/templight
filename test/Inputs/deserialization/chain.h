// Included textually into both DeserA and DeserB, so each module file holds
// its own copy of every Chain specialization it instantiates.
#ifndef DESER_CHAIN_H
#define DESER_CHAIN_H
template <int N> struct Chain {
  static constexpr int value = N + Chain<N - 1>::value;
};
template <> struct Chain<0> {
  static constexpr int value = 0;
};
#endif

#ifndef SNABEL_BOX_HPP
#define SNABEL_BOX_HPP

#include <deque>
#include <variant>

#include "snabel/iter.hpp"
#include "snackis/core/error.hpp"
#include "snackis/core/fmt.hpp"
#include "snackis/core/rat.hpp"

namespace snabel {  
  using namespace snackis;

  struct Box;
  struct Func;
  struct Exec;
  struct Fiber;
  struct Label;
  struct Scope;
  struct Thread;
  struct Type;

  struct Undef {
    bool operator ()(const Box &box) const;
  };

  using Pair = std::pair<Box, Box>;
  using PairRef = std::shared_ptr<Pair>;
  using List = std::deque<Box>;
  using ListRef = std::shared_ptr<List>;

  using Val = std::variant<Undef,
			   bool, char, int64_t, Rat, str,
			   PairRef, ListRef, Iter::Ref,
			   Fiber *, Func *, Label *, Thread *, Type *>;
  
  struct Box {
    Type *type;
    Val val;

    Box(Type &t, const Val &v);
  };

  using Stack = std::deque<Box>;

  extern const Undef undef;

  bool operator ==(const Box &x, const Box &y);
  bool operator !=(const Box &x, const Box &y);
  str dump(const List &lst);
  str list_fmt(const List &lst);
  str dump(const Pair &pr);
  str pair_fmt(const Pair &pr);
  
  template <typename T>
  const T &get(const Box &b) {
    CHECK(!undef(b), _);
    CHECK(std::holds_alternative<T>(b.val), _);
    return std::get<T>(b.val);
  }

  template <typename T>
  T &get(Box &b) {
    CHECK(!undef(b), _);
    CHECK(std::holds_alternative<T>(b.val), _);
    return std::get<T>(b.val);
  }
}

namespace snackis {
  template <>
  str fmt_arg(const snabel::Box &arg);
}

#endif

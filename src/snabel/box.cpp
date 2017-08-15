#include "snabel/box.hpp"
#include "snabel/exec.hpp"

namespace snabel {
  const Undef undef;

  bool Undef::operator ()(const Box &box) const {
    return box.val.index() == 0;
  }

  Box::Box(Type &t, const Val &v):
    type(&t), val(v)
  { }

  bool operator ==(const Box &x, const Box &y) {
    return x.type == y.type && x.type->eq(x, y);
  }
  
  bool operator !=(const Box &x, const Box &y) {
    return x.type != y.type || !x.type->eq(x, y);
  }

  IterRef make_iter(const Box &in) {
    return std::make_shared<Iter>((*in.type->iter)(in).second);
  }

  ListRef make_list() {
    return std::make_shared<List>();
  }
}

namespace snackis {
  template <>
  str fmt_arg(const snabel::Box &arg) {
    return fmt("%0|%1",
	       arg.type->dump(arg),
	       arg.type->name);
  }
}

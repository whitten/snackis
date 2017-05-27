#ifndef SNACKIS_TYPE_HPP
#define SNACKIS_TYPE_HPP

#include <iostream>

#include "snackis/core/str.hpp"
#include "snackis/core/val.hpp"

namespace snackis {
  struct BasicType {
    const str name; 
    BasicType(const str &name);
    virtual Val read_val(std::istream &in) const = 0;
    virtual void write_val(const Val &val, std::ostream &out) const = 0;
  };

  struct NullType: public BasicType {
    NullType();
    Val read_val(std::istream &in) const override;
    void write_val(const Val &val, std::ostream &out) const override;
  };

  template <typename ValT>
  struct Type: public BasicType {
    Type(const str &name);
    virtual ValT from_val(const Val &in) const = 0;
    virtual Val to_val(const ValT &in) const = 0;
    Val read_val(std::istream &in) const override;
    void write_val(const Val &val, std::ostream &out) const override;
    virtual ValT read(std::istream &in) const = 0;
    virtual void write(const ValT &val, std::ostream &out) const = 0;
  };

  extern const NullType null_type;

  template <typename ValT>
  Type<ValT>::Type(const str &name): BasicType(name) { }

  template <typename ValT>
  Val Type<ValT>::read_val(std::istream &in) const {
    return read(in);
  }
  
  template <typename ValT>
  void Type<ValT>::write_val(const Val &val, std::ostream &out) const {
    write(from_val(val), out);
  }
}

#endif

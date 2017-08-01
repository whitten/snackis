#ifndef SNABEL_TYPE_HPP
#define SNABEL_TYPE_HPP

#include "snackis/core/func.hpp"
#include "snackis/core/str.hpp"

namespace snabel {  
  using namespace snackis;

  struct Box;
  
  struct BasicType {
    const str name;
    
    func<str (const Box &)> fmt;
    
    BasicType(const str &n);
    BasicType(const BasicType &) = delete;
    virtual ~BasicType();
    const BasicType &operator =(const BasicType &) = delete;
  };

  struct TypeSeq: BasicType {
    BasicType &elem_type;
    TypeSeq(BasicType &elt);
  };

  struct Type: BasicType {
    TypeSeq seq;

    Type(const str &n);
  };
  
  bool operator <(const BasicType &x, const BasicType &y);
  bool isa(const Box &val, const BasicType &typ);
}

#endif

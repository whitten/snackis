#ifndef SNABEL_EXEC_HPP
#define SNABEL_EXEC_HPP

#include <map>

#include "snabel/fiber.hpp"
#include "snabel/label.hpp"
#include "snabel/macro.hpp"
#include "snabel/sym.hpp"
#include "snabel/type.hpp"
#include "snackis/core/uid.hpp"

namespace snabel {
  struct Exec {
    std::map<str, Macro> macros;
    std::map<UId, Fiber> fibers;
    std::deque<Type> types;
    std::map<str, Func> funcs;
    std::map<str, Label> labels;
    std::deque<str> lambdas;

    Fiber &main;
    Scope &main_scope;
    Type &any_type, &meta_type,
      &bool_type, &func_type, &i64_type, &label_type, &list_type, &str_type,
      &undef_type, &void_type;
    Sym next_sym;
    
    Exec();
    Exec(const Exec &) = delete;
    const Exec &operator =(const Exec &) = delete;
  };

  Macro &add_macro(Exec &exe, const str &n, Macro::Imp imp);
  Type &add_type(Exec &exe, const str &n);
  Type &add_type(Exec &exe, const str &n, Type &super);
  Type &get_list_type(Exec &exe, Type &elt);
  Label &add_label(Exec &exe, const str &tag);
  void clear_labels(Exec &exe);
  
  FuncImp &add_func(Exec &exe,
		    const str n,
		    const ArgTypes &args,
		    const ArgTypes &results,
		    FuncImp::Imp imp);

  Sym gensym(Exec &exe);
  Label *find_label(Exec &exe, const str &tag);
  bool run(Exec &exe, const str &in);
}

#endif

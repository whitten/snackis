#include <iostream>

#include "snabel/box.hpp"
#include "snabel/compiler.hpp"
#include "snabel/error.hpp"
#include "snabel/exec.hpp"
#include "snabel/iter.hpp"
#include "snabel/list.hpp"
#include "snabel/range.hpp"
#include "snabel/str.hpp"
#include "snabel/type.hpp"

namespace snabel {
  static void nop_imp(Scope &scp, const Args &args)
  { }

  static void is_imp(Scope &scp, const Args &args) {
    auto &v(args.at(0));
    auto &t(args.at(1));
    push(scp.thread, scp.exec.bool_type, isa(v, *get<Type *>(t)));
  }

  static void type_imp(Scope &scp, const Args &args) {
    auto &v(args.at(0));
    push(scp.thread, get_meta_type(scp.exec, *v.type), v.type);
  }

  static void eq_imp(Scope &scp, const Args &args) {
    auto &x(args.at(0)), &y(args.at(1));
    push(scp.thread, scp.exec.bool_type, x.type->eq(x, y));
  }

  static void equal_imp(Scope &scp, const Args &args) {
    auto &x(args.at(0)), &y(args.at(1));
    push(scp.thread, scp.exec.bool_type, x.type->equal(x, y));
  }
  
  static void zero_i64_imp(Scope &scp, const Args &args) {
    bool res(get<int64_t>(args.at(0)) == 0);
    push(scp.thread, scp.exec.bool_type, res);
  }

  static void inc_i64_imp(Scope &scp, const Args &args) {
    auto &a(args.at(0));
    push(scp.thread, *a.type, get<int64_t>(a)+1);
  }

  static void dec_i64_imp(Scope &scp, const Args &args) {
    auto &a(args.at(0));
    push(scp.thread, *a.type, get<int64_t>(a)-1);
  }

  static void add_i64_imp(Scope &scp, const Args &args) {
    auto &x(get<int64_t>(args.at(0))), &y(get<int64_t>(args.at(1)));
    push(scp.thread, scp.exec.i64_type, x+y);
  }

  static void sub_i64_imp(Scope &scp, const Args &args) {
    auto &x(get<int64_t>(args.at(0))), &y(get<int64_t>(args.at(1)));
    push(scp.thread, scp.exec.i64_type, x-y);
  }

  static void mul_i64_imp(Scope &scp, const Args &args) {
    auto &x(get<int64_t>(args.at(0))), &y(get<int64_t>(args.at(1)));
    push(scp.thread, scp.exec.i64_type, x*y);
  }

  static void div_i64_imp(Scope &scp, const Args &args) {
    auto &num(get<int64_t>(args.at(0)));
    auto &div(get<int64_t>(args.at(1)));
    bool neg = (num < 0 && div > 0) || (div < 0 && num >= 0);
    push(scp.thread, scp.exec.rat_type, Rat(abs(num), abs(div), neg));
  }

  static void mod_i64_imp(Scope &scp, const Args &args) {
    auto &x(get<int64_t>(args.at(0))), &y(get<int64_t>(args.at(1)));
    push(scp.thread, scp.exec.i64_type, x%y);
  }

  static void trunc_imp(Scope &scp, const Args &args) {
    push(scp.thread, scp.exec.i64_type, trunc(get<Rat>(args.at(0))));
  }

  static void frac_imp(Scope &scp, const Args &args) {
    push(scp.thread, scp.exec.rat_type, frac(get<Rat>(args.at(0))));
  }

  static void add_rat_imp(Scope &scp, const Args &args) {
    auto &x(get<Rat>(args.at(0)));
    auto &y(get<Rat>(args.at(1)));
    push(scp.thread, scp.exec.rat_type, x+y);
  }

  static void sub_rat_imp(Scope &scp, const Args &args) {
    auto &x(get<Rat>(args.at(0)));
    auto &y(get<Rat>(args.at(1)));
    push(scp.thread, scp.exec.rat_type, x-y);
  }

  static void mul_rat_imp(Scope &scp, const Args &args) {
    auto &x(get<Rat>(args.at(0)));
    auto &y(get<Rat>(args.at(1)));
    push(scp.thread, scp.exec.rat_type, x*y);
  }

  static void div_rat_imp(Scope &scp, const Args &args) {
    auto &x(get<Rat>(args.at(0)));
    auto &y(get<Rat>(args.at(1)));
    push(scp.thread, scp.exec.rat_type, x/y);
  }

  static void opt_imp(Scope &scp, const Args &args) {
    auto &in(args.at(0));
    push(scp.thread, get_opt_type(scp.exec, *in.type), in.val);
  }

  static void opt_or_imp(Scope &scp, const Args &args) {
    auto &in(args.at(0));
    auto &alt(args.at(1));

    if (empty(in)) {
      push(scp.thread, alt);
    } else {
      push(scp.thread, *in.type->args.at(0), in.val);
    }
  }

  static void opt_or_opt_imp(Scope &scp, const Args &args) {
    auto &in(args.at(0));
    auto &alt(args.at(1));
    push(scp.thread, empty(in) ? alt : in);
  }

  static void iter_imp(Scope &scp, const Args &args) {
    auto &in(args.at(0));
    auto it((*in.type->iter)(in));
    push(scp.thread, it->type, it);
  }

  static void iterable_join_imp(Scope &scp, const Args &args) {    
    auto &in(args.at(0));
    auto it((*in.type->iter)(in));
    auto &sep(args.at(1));
    OutStream out;
    bool first(true);
    
    while (true) {
      auto v(it->next());
      if (!v) { break; }
      if (!first) { out << sep.type->fmt(sep); }
      out << v->type->fmt(*v);
      first = false;
    }
    
    push(scp.thread, scp.exec.str_type, out.str()); 
  }

  static void iterable_list_imp(Scope &scp, const Args &args) {
    auto &in(args.at(0));
    auto it((*in.type->iter)(in));
    auto out(std::make_shared<List>());
    
    while (true) {
      auto v(it->next());
      if (!v) { break; }
      out->push_back(*v);
    }
    
    push(scp.thread, get_list_type(scp.exec, *it->type.args.at(0)), out); 
  }

  static void iterable_zip_imp(Scope &scp, const Args &args) {
    auto &exe(scp.exec);
    auto &x(args.at(0)), &y(args.at(1));
    auto xi((*x.type->iter)(x)), yi((*y.type->iter)(y));

    push(scp.thread,
	 get_iter_type(exe, get_pair_type(exe,
					  *xi->type.args.at(0),
					  *yi->type.args.at(0))),
	 Iter::Ref(new ZipIter(exe, xi, yi)));
  }

  static void list_imp(Scope &scp, const Args &args) {
    auto &elt(args.at(0));
    push(scp.thread,
	 get_list_type(scp.exec, *get<Type *>(elt)),
	 std::make_shared<List>());    
  }

  static void list_push_imp(Scope &scp, const Args &args) {
    auto &lst(args.at(0));
    auto &el(args.at(1));
    get<ListRef>(lst)->push_back(el);
    push(scp.thread, lst);    
  }

  static void list_pop_imp(Scope &scp, const Args &args) {
    auto &lst_arg(args.at(0));
    auto &lst(*get<ListRef>(lst_arg));
    push(scp.thread, lst_arg);
    push(scp.thread, lst.back());
    lst.pop_back();
  }

  static void list_reverse_imp(Scope &scp, const Args &args) {
    auto &in_arg(args.at(0));
    auto &in(*get<ListRef>(in_arg));
    ListRef out(new List());
    std::copy(in.rbegin(), in.rend(), std::back_inserter(*out));
    push(scp.thread, *in_arg.type, out); 
  }

  static void list_unzip_imp(Scope &scp, const Args &args) {
    auto &exe(scp.exec);
    auto &in(args.at(0));

    auto &yt(*in.type->args.at(0)->args.at(0));
    push(scp.thread,
	 get_iter_type(exe, yt),
	 Iter::Ref(new ListIter(exe, yt, get<ListRef>(in), [](auto &el) {
	       return get<PairRef>(el)->first;
	     })));

    auto &xt(*in.type->args.at(0)->args.at(1));
    push(scp.thread,
	 get_iter_type(exe, xt),
	 Iter::Ref(new ListIter(exe, xt, get<ListRef>(in), [](auto &el) {
	       return get<PairRef>(el)->second;
	     })));
  }

  static void zip_imp(Scope &scp, const Args &args) {
    auto &l(args.at(0)), &r(args.at(1));
    
    push(scp.thread,
	 get_pair_type(scp.exec, *l.type, *r.type),
	 std::make_shared<Pair>(l, r));    
  }

  static void unzip_imp(Scope &scp, const Args &args) {
    auto &p(*get<PairRef>(args.at(0)));   
    push(scp.thread, p.first);
    push(scp.thread, p.second);
  }

  static void fiber_imp(Scope &scp, const Args &args) {
    auto &thd(scp.thread);
    auto &f(add_fiber(thd, *get<Label *>(args.at(0))));
    push(thd, scp.exec.fiber_type, &f);
  }
  
  static void fiber_result_imp(Scope &scp, const Args &args) {
    auto &thd(scp.thread);
    auto &f(*get<Fiber *>(args.at(0)));
    push(thd, make_opt(scp.exec, f.result));
  }

  static void thread_imp(Scope &scp, const Args &args) {
    auto &t(start_thread(scp, args.at(0)));
    push(scp.thread, scp.exec.thread_type, &t);
  }

  static void thread_join_imp(Scope &scp, const Args &args) {
    join(*get<Thread *>(args.at(0)), scp);
  }

  Exec::Exec():
    main(threads.emplace(std::piecewise_construct,
				std::forward_as_tuple(0),
				std::forward_as_tuple(*this, 0)).first->second),
    main_scope(main.scopes.at(0)),
    meta_type("Type<Any>"),
    any_type(add_type(*this, "Any")),
    bool_type(add_type(*this, "Bool")),
    callable_type(add_type(*this, "Callable")),
    char_type(add_type(*this, "Char")),
    fiber_type(add_type(*this, "Fiber")),    
    func_type(add_type(*this, "Func")),
    i64_type(add_type(*this, "I64")),
    iter_type(add_type(*this, "Iter")),
    iterable_type(add_type(*this, "Iterable")),
    label_type(add_type(*this, "Label")),
    lambda_type(add_type(*this, "Lambda")),
    list_type(add_type(*this, "List")),
    opt_type(add_type(*this, "Opt")),
    pair_type(add_type(*this, "Pair")),
    rat_type(add_type(*this, "Rat")),
    str_type(add_type(*this, "Str")),
    thread_type(add_type(*this, "Thread")),
    void_type(add_type(*this, "Void")),
    next_gensym(1)
  {    
    any_type.fmt = [](auto &v) { return "n/a"; };
    any_type.eq = [](auto &x, auto &y) { return false; };

    meta_type.supers.push_back(&any_type);
    meta_type.args.push_back(&any_type);
    meta_type.fmt = [](auto &v) { return get<Type *>(v)->name; };
    meta_type.eq = [](auto &x, auto &y) { return get<Type *>(x) == get<Type *>(y); };

    void_type.fmt = [](auto &v) { return "n/a"; };
    void_type.eq = [](auto &x, auto &y) { return true; };  

    opt_type.supers.push_back(&any_type);
    opt_type.args.push_back(&any_type);
    opt_type.dump = [](auto &v) -> str {
      if (empty(v)) { return "#n/a"; }
      return v.type->args.at(0)->dump(v);
    };
    opt_type.fmt = [](auto &v) -> str {
      if (empty(v)) { return "#n/a"; }
      return v.type->args.at(0)->fmt(v);
    };    
    opt_type.eq = [](auto &x, auto &y) {
      if (empty(x) && !empty(y)) { return true; }
      if (!empty(x) || !empty(y)) { return false; }
      return x.type->eq(x, y);
    };
    opt_type.equal = [](auto &x, auto &y) {
      if (empty(x) && !empty(y)) { return true; }
      if (!empty(x) || !empty(y)) { return false; }
      return x.type->equal(x, y);
    };
    put_env(main_scope, "#n/a", Box(opt_type, empty_val));

    callable_type.supers.push_back(&any_type);
    callable_type.args.push_back(&any_type);
    callable_type.fmt = [](auto &v) { return "n/a"; };
    callable_type.eq = [](auto &x, auto &y) { return false; };

    iter_type.supers.push_back(&any_type);
    iter_type.args.push_back(&any_type);
    iter_type.fmt = [](auto &v) { return "n/a"; };
    iter_type.eq = [](auto &x, auto &y) { return false; };

    iterable_type.supers.push_back(&any_type);
    iterable_type.args.push_back(&any_type);
    iterable_type.fmt = [](auto &v) { return "n/a"; };
    iterable_type.eq = [](auto &x, auto &y) { return false; };

    list_type.supers.push_back(&any_type);
    list_type.args.push_back(&any_type);
    list_type.dump = [](auto &v) { return dump(*get<ListRef>(v)); };
    list_type.fmt = [](auto &v) { return list_fmt(*get<ListRef>(v)); };
    list_type.eq = [](auto &x, auto &y) { return false; };

    pair_type.supers.push_back(&any_type);
    pair_type.args.push_back(&any_type);
    pair_type.args.push_back(&any_type);
    pair_type.dump = [](auto &v) { return dump(*get<PairRef>(v)); };
    pair_type.fmt = [](auto &v) { return pair_fmt(*get<PairRef>(v)); };
    pair_type.eq = [](auto &x, auto &y) { 
      return get<PairRef>(x) == get<PairRef>(y); 
    };
    pair_type.equal = [](auto &x, auto &y) { 
      return *get<PairRef>(x) == *get<PairRef>(y); 
    };

    bool_type.supers.push_back(&any_type);
    bool_type.fmt = [](auto &v) { return get<bool>(v) ? ":t" : ":f"; };
    bool_type.eq = [](auto &x, auto &y) { return get<bool>(x) == get<bool>(y); };
    put_env(main_scope, "#t", Box(bool_type, true));
    put_env(main_scope, "#f", Box(bool_type, false));

    char_type.supers.push_back(&any_type);

    char_type.dump = [](auto &v) -> str {
      auto c(get<char>(v));
      
      switch (c) {
      case ' ':
	return "\\space";
      case '\n':
	return "\\n";
      case '\t':
	return "\\t";
      }

      return fmt("\\%0", str(1, c));
    };

    char_type.fmt = [](auto &v) { return str(1, get<char>(v)); };
    
    char_type.eq = [](auto &x, auto &y) { return get<char>(x) == get<char>(y); };
    
    fiber_type.supers.push_back(&any_type);
    fiber_type.supers.push_back(&callable_type);
    fiber_type.fmt = [](auto &v) { return fmt("fiber(%0)", get<Fiber *>(v)->id); };
    fiber_type.eq = [](auto &x, auto &y) {
      return get<Fiber *>(x) == get<Fiber *>(y);
    };

    fiber_type.call.emplace([](auto &scp, auto &v) {
	call(*get<Fiber *>(v), scp);
	return true;
      });

    func_type.supers.push_back(&any_type);
    func_type.supers.push_back(&callable_type);
    func_type.fmt = [](auto &v) { return fmt("func(%0)", get<Func *>(v)->name); };
    func_type.eq = [](auto &x, auto &y) { return get<Func *>(x) == get<Func *>(y); };
    
    func_type.call.emplace([](auto &scp, auto &v) {
	auto &thd(scp.thread);
	auto &fn(*get<Func *>(v));
	auto m(match(fn, thd));

	if (!m) {
	  ERROR(Snabel, fmt("Function not applicable: %0\n%1", 
			    fn.name, curr_stack(thd)));
	  return false;
	}

	(*m->first)(scp, m->second);
	return true;
      });

    i64_type.supers.push_back(&any_type);
    i64_type.supers.push_back(&get_iterable_type(*this, i64_type));
    i64_type.fmt = [](auto &v) { return fmt_arg(get<int64_t>(v)); };
    i64_type.eq = [](auto &x, auto &y) { return get<int64_t>(x) == get<int64_t>(y); };

    i64_type.iter = [this](auto &in) {
      return Iter::Ref(new RangeIter(*this, Range(0, get<int64_t>(in))));
    };
    
    label_type.supers.push_back(&any_type);
    label_type.supers.push_back(&callable_type);
    label_type.fmt = [](auto &v) {
      auto &l(*get<Label *>(v));
      return fmt("%0:%1", l.tag, l.pc);
    };

    label_type.eq = [](auto &x, auto &y) {
      return get<Label *>(x) == get<Label *>(y);
    };

    label_type.call.emplace([](auto &scp, auto &v) {
	jump(scp, *get<Label *>(v));
	return true;
      });

    lambda_type.supers.push_back(&any_type);
    lambda_type.supers.push_back(&callable_type);

    lambda_type.fmt = [](auto &v) {
      auto &l(*get<Label *>(v));
      return fmt("lambda(%0:%1)", l.tag, l.pc);
    };
    
    lambda_type.eq = [](auto &x, auto &y) {
      return get<Label *>(x) == get<Label *>(y);
    };

    lambda_type.call.emplace([](auto &scp, auto &v) {
	call(scp, *get<Label *>(v));
	return true;
      });

    str_type.supers.push_back(&any_type);
    str_type.supers.push_back(&get_iterable_type(*this, char_type));
    str_type.fmt = [](auto &v) { return fmt("'%0'", get<str>(v)); };
    str_type.eq = [](auto &x, auto &y) { return get<str>(x) == get<str>(y); };

    str_type.iter = [this](auto &in) {
      return Iter::Ref(new StrIter(*this, get<str>(in))); };

    rat_type.supers.push_back(&any_type);
    rat_type.fmt = [](auto &v) { return fmt_arg(get<Rat>(v)); };
    rat_type.eq = [](auto &x, auto &y) { return get<Rat>(x) == get<Rat>(y); };
    
    thread_type.supers.push_back(&any_type);
    thread_type.fmt = [](auto &v) { return fmt("thread_%0", get<Thread *>(v)->id); };
    thread_type.eq = [](auto &x, auto &y) {
      return get<Thread *>(x) == get<Thread *>(y);
    };

    add_conv(*this, i64_type, rat_type, [this](auto &v) {	
	v.type = &rat_type;
	auto n(get<int64_t>(v));
	v.val = Rat(abs(n), 1, n < 0);
	return true;
      });
    
    add_func(*this, "nop", {}, {}, nop_imp);

    add_func(*this, "is?",
	     {ArgType(any_type), ArgType(meta_type)}, {ArgType(bool_type)},
	     is_imp);
   
    add_func(*this, "type",
	     {ArgType(any_type)}, {ArgType(0)},
	     type_imp);
   
    add_func(*this, "=",
	     {ArgType(any_type), ArgType(0)}, {ArgType(bool_type)},
	     eq_imp);
   
    add_func(*this, "==",
	     {ArgType(any_type), ArgType(0)}, {ArgType(bool_type)},
	     equal_imp);
   
    add_func(*this, "zero?",
	     {ArgType(i64_type)}, {ArgType(bool_type)},
	     zero_i64_imp);
   
    add_func(*this, "inc",
	     {ArgType(i64_type)}, {ArgType(i64_type)},
	     inc_i64_imp);

    add_func(*this, "dec",
	     {ArgType(i64_type)}, {ArgType(i64_type)},
	     dec_i64_imp);

    add_func(*this, "+",
	     {ArgType(i64_type), ArgType(i64_type)}, {ArgType(i64_type)},
	     add_i64_imp);
    add_func(*this, "-",
	     {ArgType(i64_type), ArgType(i64_type)}, {ArgType(i64_type)},
	     sub_i64_imp);
    add_func(*this, "*",
	     {ArgType(i64_type), ArgType(i64_type)}, {ArgType(i64_type)},
	     mul_i64_imp);
    add_func(*this, "/",
	     {ArgType(i64_type), ArgType(i64_type)}, {ArgType(rat_type)},
	     div_i64_imp);
    add_func(*this, "%",
	     {ArgType(i64_type), ArgType(i64_type)}, {ArgType(i64_type)},
	     mod_i64_imp);

    add_func(*this, "trunc",
	     {ArgType(rat_type)}, {ArgType(i64_type)},
	     trunc_imp);
    add_func(*this, "frac",
	     {ArgType(rat_type)}, {ArgType(rat_type)},
	     frac_imp);
    add_func(*this, "+",
	     {ArgType(rat_type), ArgType(rat_type)}, {ArgType(rat_type)},
	     add_rat_imp);
    add_func(*this, "-",
	     {ArgType(rat_type), ArgType(rat_type)}, {ArgType(rat_type)},
	     sub_rat_imp);
    add_func(*this, "*",
	     {ArgType(rat_type), ArgType(rat_type)}, {ArgType(rat_type)},
	     mul_rat_imp);
    add_func(*this, "/",
	     {ArgType(rat_type), ArgType(rat_type)}, {ArgType(rat_type)},
	     div_rat_imp);

    add_func(*this, "opt",
	     {ArgType(any_type)},
	     {ArgType([this](auto &args) {
		   return &get_opt_type(*this, *args.at(0).type);
		 })},
	     opt_imp);

    add_func(*this, "or",
	     {ArgType(opt_type),
		 ArgType([](auto &args) { return args.at(0).type->args.at(0); })},
	     {ArgType([](auto &args) { return args.at(0).type->args.at(0); })},
	     opt_or_imp);

    add_func(*this, "or",
	     {ArgType(opt_type), ArgType(0)}, {ArgType(0)},
	     opt_or_opt_imp);

    add_func(*this, "iter",
	     {ArgType(iterable_type)},
	     {ArgType([this](auto &args) { 
		   return &get_iter_type(*this, *args.at(0).type->args.at(0)); 
		 })},
	     iter_imp);

    add_func(*this, "join",
	     {ArgType(iterable_type), ArgType(any_type)}, {ArgType(str_type)},
	     iterable_join_imp);

    add_func(*this, "list",
	     {ArgType(iterable_type)},
	     {ArgType([this](auto &args) {
		   return &get_list_type(*this, *args.at(0).type->args.at(0));
		 })},
	     iterable_list_imp);

    add_func(*this, "zip",
	     {ArgType(iterable_type), ArgType(iterable_type)},
	     {ArgType([this](auto &args) {
		   return &get_iter_type(*this,
					 get_pair_type(*this,
						       *args.at(0).type->args.at(0),
						       *args.at(1).type->args.at(0)));
		 })},			
	     iterable_zip_imp);

    add_func(*this, "list",
	     {ArgType(meta_type)},
	     {ArgType([this](auto &args) {
		   return &get_list_type(*this, *args.at(0).type);
		 })},
	     list_imp);
    add_func(*this, "push",
	     {ArgType(list_type), ArgType(0, 0)}, {ArgType(0)},
	     list_push_imp);
    add_func(*this, "pop",
	     {ArgType(list_type)}, {ArgType(0), ArgType(0, 0)},
	     list_pop_imp);
    add_func(*this, "reverse",
	     {ArgType(list_type)}, {ArgType(0)},
	     list_reverse_imp);

    add_func(*this, "unzip",
	     {ArgType(get_list_type(*this, pair_type))},
	     {ArgType([this](auto &args) {
		   return &get_iter_type(*this,
					 *args.at(0).type->args.at(0)->args.at(0));
		 }),
		 ArgType([this](auto &args) {
		     return &get_iter_type(*this,
					   *args.at(0).type->args.at(0)->args.at(1));
		   })},			
	     list_unzip_imp);

    add_func(*this, ".",
	     {ArgType(any_type), ArgType(any_type)},
	     {ArgType([this](auto &args) {
		   return &get_pair_type(*this, *args.at(0).type, *args.at(1).type);
		 })},			
	     zip_imp);
    
    add_func(*this, "unzip",
	     {ArgType(pair_type)}, {ArgType(0, 0), ArgType(0, 1)},
	     unzip_imp);
	     
    add_func(*this, "fiber",
	     {ArgType(lambda_type)}, {ArgType(fiber_type)},
	     fiber_imp);

    add_func(*this, "result",
	     {ArgType(fiber_type)}, {ArgType(opt_type)},
	     fiber_result_imp);

    add_func(*this, "thread",
	     {ArgType(callable_type)}, {ArgType(thread_type)},
	     thread_imp);

    add_func(*this, "join",
	     {ArgType(thread_type)}, {ArgType(any_type)},
	     thread_join_imp);

    add_macro(*this, "!", [](auto pos, auto &in, auto &out) {	
	out.emplace_back(Check());
      });
    
    add_macro(*this, "{", [](auto pos, auto &in, auto &out) {
	out.emplace_back(Lambda());
      });

    add_macro(*this, "}", [](auto pos, auto &in, auto &out) {
	out.emplace_back(Unlambda());
      });

    add_macro(*this, "(", [](auto pos, auto &in, auto &out) {
	out.emplace_back(Backup(true));
      });

    add_macro(*this, ")", [](auto pos, auto &in, auto &out) {
	out.emplace_back(Restore());
      });

    add_macro(*this, "[", [](auto pos, auto &in, auto &out) {
	out.emplace_back(Backup());
      });

    add_macro(*this, "]", [](auto pos, auto &in, auto &out) {
	out.emplace_back(Stash());	
	out.emplace_back(Restore());
      });

    add_macro(*this, "<", [](auto pos, auto &in, auto &out) {
	out.emplace_back(Param());
      });

    add_macro(*this, ">", [](auto pos, auto &in, auto &out) {
	out.emplace_back(Unparam());
      });

    add_macro(*this, "func:", [this](auto pos, auto &in, auto &out) {
	if (in.size() < 2) {
	  ERROR(Snabel, fmt("Malformed func on row %0, col %1",
			    pos.row, pos.col));
	} else {
	  out.emplace_back(Backup());
	  const str n(in.at(0).text);
	  auto start(std::next(in.begin()));
	  auto end(find_end(start, in.end()));
	  compile(*this, TokSeq(start, end), out);
	  if (end != in.end()) { end++; }
	  in.erase(in.begin(), end);
	  out.emplace_back(Restore());
	  out.emplace_back(Putenv(n));
	}
      });

    add_macro(*this, "label:", [this](auto pos, auto &in, auto &out) {
	if (in.empty()) {
	  ERROR(Snabel, fmt("Malformed label on row %0, col %1",
			    pos.row, pos.col));
	  return;
	}

	out.emplace_back(Backup(true));
	const str tag(in.at(0).text);
	in.pop_front();

	if (!in.empty()) {
	  if (in.at(0).text != ";") {
	    ERROR(Snabel, fmt("Malformed label on row %0, col %1",
				pos.row, pos.col));
	    return;
	  }

	  in.pop_front();
	}

	out.emplace_back(Target(add_label(*this, tag)));
      });
    
    add_macro(*this, "let:", [this](auto pos, auto &in, auto &out) {
	if (in.empty()) {
	  ERROR(Snabel, fmt("Malformed let on row %0, col %1",
			    pos.row, pos.col));
	} else {
	  out.emplace_back(Backup(true));
	  const str n(in.at(0).text);
	  auto start(std::next(in.begin()));
	  auto end(find_end(start, in.end()));
	  compile(*this, TokSeq(start, end), out);
	  if (end != in.end()) { end++; }
	  in.erase(in.begin(), end);
	  out.emplace_back(Restore());
	  out.emplace_back(Putenv(fmt("@%0", n)));
	}
      });

    add_macro(*this, "call", [](auto pos, auto &in, auto &out) {
	out.emplace_back(Call());
      });

    add_macro(*this, "_", [](auto pos, auto &in, auto &out) {
	out.emplace_back(Drop(1));
      });

    add_macro(*this, "for", [](auto pos, auto &in, auto &out) {
	out.emplace_back(For());
      });

    add_macro(*this, "recall", [](auto pos, auto &in, auto &out) {
	out.emplace_back(Recall());
      });

    add_macro(*this, "|", [](auto pos, auto &in, auto &out) {
	out.emplace_back(Reset());
      });

    add_macro(*this, "return", [](auto pos, auto &in, auto &out) {
	out.emplace_back(Return(false));
      });

    add_macro(*this, "$", [](auto pos, auto &in, auto &out) {
	out.emplace_back(Stash());
      });

    add_macro(*this, "when", [this](auto pos, auto &in, auto &out) {
	out.emplace_back(Branch());
      });

    add_macro(*this, "yield", [this](auto pos, auto &in, auto &out) {
	out.emplace_back(Yield());
      });
  }

  Macro &add_macro(Exec &exe, const str &n, Macro::Imp imp) {
    return exe.macros.emplace(std::piecewise_construct,
			      std::forward_as_tuple(n),
			      std::forward_as_tuple(n, imp)).first->second; 
  }
  
  Type &get_meta_type(Exec &exe, Type &t) {    
    str n(fmt("Type<%0>", t.name));
    auto fnd(find_type(exe, n));
    if (fnd) { return *fnd; }
    auto &mt(add_type(exe, n));
    mt.raw = &exe.meta_type;
    mt.supers.push_back(&exe.meta_type);
    mt.args.push_back(&t);
    mt.fmt = [](auto &v) { return get<Type *>(v)->name; };
    mt.eq = [](auto &x, auto &y) { return get<Type *>(x) == get<Type *>(y); };
    return mt;
  }

  Type &add_type(Exec &exe, const str &n) {
    return exe.types.emplace(std::piecewise_construct,
			     std::forward_as_tuple(n),
			     std::forward_as_tuple(n)).first->second; 
  }

  Type *find_type(Exec &exe, const str &n) {
    auto fnd(exe.types.find(n));
    if (fnd == exe.types.end()) { return nullptr; }
    return &fnd->second;
  }

  Type &get_type(Exec &exe, Type &raw, Types args) {
    if (args.empty()) { return raw; }
    
    if (args.size() > raw.args.size()) {
      ERROR(Snabel, fmt("Too many params for type %0", raw.name));
      return raw;
    }

    for (size_t i(args.size()); i < raw.args.size(); i++) {
      args.push_back(raw.args.at(i));
    }
		  
    if (&raw == &exe.iter_type) {
      return get_iter_type(exe, *args.at(0));
    } else if (&raw == &exe.iterable_type) {      
      return get_iterable_type(exe, *args.at(0));
    } else if (&raw == &exe.list_type) {      
      return get_list_type(exe, *args.at(0));
    } else if (&raw == &exe.pair_type) {      
      return get_pair_type(exe, *args.at(0), *args.at(1));
    }

    ERROR(Snabel, fmt("Invalid type: %1", raw.name));
    return raw;
  }

  Type &get_opt_type(Exec &exe, Type &elt) {    
    str n(fmt("Opt<%0>", elt.name));
    auto fnd(find_type(exe, n));
    if (fnd) { return *fnd; }
    auto &t(add_type(exe, n));
    t.raw = &exe.opt_type;
    t.supers.push_back(&exe.any_type);
    t.supers.push_back(&exe.opt_type);
    t.args.push_back(&elt);
    t.fmt = exe.opt_type.fmt;
    t.dump = exe.opt_type.dump;
    t.eq = exe.opt_type.eq;
    t.equal = exe.opt_type.equal;
    return t;
  }

  Type &get_iter_type(Exec &exe, Type &elt) {    
    str n(fmt("Iter<%0>", elt.name));
    auto fnd(find_type(exe, n));
    if (fnd) { return *fnd; }
    auto &t(add_type(exe, n));
    t.raw = &exe.iter_type;
    t.supers.push_back(&exe.any_type);
    t.supers.push_back(&get_iterable_type(exe, elt));    
    t.supers.push_back(&exe.iter_type);
    t.args.push_back(&elt);
    t.fmt = [&elt](auto &v) { return "n/a"; };

    t.eq = [&elt](auto &x, auto &y) {
      return get<Iter::Ref>(x) == get<Iter::Ref>(y);
    };

    t.equal = [&exe](auto &x, auto &y) {
      auto xi(get<Iter::Ref>(x)), yi(get<Iter::Ref>(y));
      opt<Box> xv, yv;
      
      while (true) {
	xv = xi->next();
	yv = yi->next();
	if (!xv || !yv) { break; }
	if (xv->type != yv->type || !xv->type->equal(*xv, *yv)) { return false; }
      }

      return !xv && !yv;
    };
    
    t.iter = [&exe](auto &in) { return get<Iter::Ref>(in); };
    return t;
  }

  Type &get_iterable_type(Exec &exe, Type &elt) {    
    str n(fmt("Iterable<%0>", elt.name));
    auto fnd(find_type(exe, n));
    if (fnd) { return *fnd; }
    auto &t(add_type(exe, n));
    t.raw = &exe.iterable_type;
    t.supers.push_back(&exe.any_type);
    t.supers.push_back(&exe.iterable_type);
    t.args.push_back(&elt);
    t.fmt = [&elt](auto &v) { return "n/a"; };
    t.eq = [&elt](auto &x, auto &y) { return false; };
    return t;
  }
  
  Type &get_list_type(Exec &exe, Type &elt) {    
    str n(fmt("List<%0>", elt.name));
    auto fnd(find_type(exe, n));
    if (fnd) { return *fnd; }
    auto &t(add_type(exe, n));
    t.raw = &exe.list_type;
    t.supers.push_back(&exe.any_type);
    t.supers.push_back(&get_iterable_type(exe, elt));
    t.supers.push_back(&exe.list_type);
    t.args.push_back(&elt);
    
    t.dump = [](auto &v) { return dump(*get<ListRef>(v)); };
    t.fmt = [](auto &v) { return list_fmt(*get<ListRef>(v)); };
    t.eq = [](auto &x, auto &y) { return get<ListRef>(x) == get<ListRef>(y); };

    t.equal = [](auto &x, auto &y) {
      auto &xs(*get<ListRef>(x)), &ys(*get<ListRef>(y));
      if (xs.size() != ys.size()) { return false; }
      auto xi(xs.begin()), yi(ys.begin());

      for (; xi != xs.end() && yi != ys.end(); xi++, yi++) {
	if (xi->type != yi->type || !xi->type->equal(*xi, *yi)) { return false; }
      }

      return true;
    };

    t.iter = [&exe, &elt](auto &in) {
      return Iter::Ref(new ListIter(exe, elt, get<ListRef>(in)));
    };
	     
    return t;
  }

  Type &get_pair_type(Exec &exe, Type &lt, Type &rt) {    
    str n(fmt("Pair<%0 %1>", lt.name, rt.name));
    auto fnd(find_type(exe, n));
    if (fnd) { return *fnd; }
    
    auto &t(add_type(exe, n));
    t.raw = &exe.pair_type;
    t.supers.push_back(&exe.any_type);
    t.supers.push_back(&exe.pair_type);
    t.args.push_back(&lt);
    t.args.push_back(&rt);

    t.dump = [](auto &v) { return dump(*get<PairRef>(v)); };
    t.fmt = [](auto &v) { return pair_fmt(*get<PairRef>(v)); };
    t.eq = [](auto &x, auto &y) { return get<PairRef>(x) == get<PairRef>(y); };
    t.equal = [](auto &x, auto &y) { return *get<PairRef>(x) == *get<PairRef>(y); };
    return t;
  }
  
  FuncImp &add_func(Exec &exe,
		    const str n,
		    const ArgTypes &args,
		    const ArgTypes &results,
		    FuncImp::Imp imp) {
    auto fnd(exe.funcs.find(n));

    if (fnd == exe.funcs.end()) {
      auto &fn(exe.funcs.emplace(std::piecewise_construct,
				  std::forward_as_tuple(n),
				  std::forward_as_tuple(n)).first->second);
      put_env(exe.main_scope, n, Box(exe.func_type, &fn));
      return add_imp(fn, args, results, imp);
    }
    
    return add_imp(fnd->second, args, results, imp);
  }

  Label &add_label(Exec &exe, const str &tag) {
    auto &l(exe.labels
      .emplace(std::piecewise_construct,
	       std::forward_as_tuple(tag),
	       std::forward_as_tuple(tag))
	    .first->second);
    put_env(exe.main_scope, tag, Box(exe.label_type, &l));
    return l;
  }

  void clear_labels(Exec &exe) {
    for (auto &l: exe.labels) {
      rem_env(exe.main_scope, l.first);
    }
    
    exe.labels.clear();
  }

  Label *find_label(Exec &exe, const str &tag) {
    auto fnd(exe.labels.find(tag));
    if (fnd == exe.labels.end()) { return nullptr; }
    return &fnd->second;
  }

  void add_conv(Exec &exe, Type &from, Type &to, Conv conv) {
    exe.convs.emplace(std::piecewise_construct,
		      std::forward_as_tuple(&from, &to),
		      std::forward_as_tuple(conv));
  }
  
  bool conv(Exec &exe, Box &val, Type &type) {
    auto fnd(exe.convs.find(std::make_pair(val.type, &type)));
    if (fnd == exe.convs.end()) { return false; }
    return fnd->second(val);
  }

  Sym gensym(Exec &exe) {
    return exe.next_gensym.fetch_add(1);
  }

  Box make_opt(Exec &exe, opt<Box> in) {
    return in
      ? Box(get_opt_type(exe, *in->type), in->val)
      : Box(exe.opt_type, empty_val);
  }

  void rewind(Exec &exe) {
    for (auto i(exe.threads.begin()); i != exe.threads.end();) {
      if (i->first == exe.main.id) {
	i++;
      } else {
	i = exe.threads.erase(i);
      }
    }

    auto &thd(exe.main);
    while (thd.scopes.size() > 1) { thd.scopes.pop_back(); }
    while (thd.stacks.size() > 1) { thd.stacks.pop_back(); }
    thd.main_scope.coros.clear();
    thd.main_scope.recalls.clear();
    thd.main_scope.return_pc = -1;
    thd.stacks.front().clear();
    thd.pc = 0;
  }

  bool compile(Exec &exe, const str &in) {
    Exec::Lock lock(exe.mutex);
    
    exe.main.ops.clear();
    clear_labels(exe);
    rewind(exe);
    size_t lnr(0);
    TokSeq toks;
    
    for (auto &ln: parse_lines(in)) {
      if (!ln.empty()) { parse_expr(ln, lnr, toks); }
      lnr++;
    }

    compile(exe, toks, exe.main.ops);
    TRY(try_compile);

    while (true) {
      rewind(exe);
      
      for (auto &op: exe.main.ops) {
	if ((!op.prepared && !prepare(op, exe.main_scope)) ||
	    !try_compile.errors.empty()) {
	  goto exit;
	}
	
	exe.main.pc++;
      }

      exe.main.pc = 0;
      exe.lambdas.clear();
      bool done(false);
      
      while (!done) {
	done = true;
	
	for (auto &op: exe.main.ops) {
	  if (refresh(op, exe.main_scope)) { done = false; }
	  if (!try_compile.errors.empty()) { goto exit; }
	  exe.main.pc++;
	}
      }

      OpSeq out;
      done = true;

      for (auto &op: exe.main.ops) {
	if (compile(op, exe.main_scope, out)) { done = false; }
	if (!try_compile.errors.empty()) { goto exit; }
      }

      if (done) { break; }
      exe.main.ops.clear();
      exe.main.ops.swap(out);
      try_compile.errors.clear();
    }

    {
      OpSeq out;
      exe.main.pc = 0;
      
      for (auto &op: exe.main.ops) {
	if (!finalize(op, exe.main_scope, out)) {
	  exe.main.pc++;
	}
	
	if (!try_compile.errors.empty()) { goto exit; }
      }
      
      exe.main.ops.clear();
      exe.main.ops.swap(out);
    }
  exit:
    rewind(exe);
    return try_compile.errors.empty();
  }
  
  bool run(Exec &exe, const str &in) {
    rewind(exe);
    if (!compile(exe, in)) { return false; }
    return run(exe.main);
  }
}

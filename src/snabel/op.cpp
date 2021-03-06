#include <algorithm>
#include <iostream>

#include "snabel/box.hpp"
#include "snabel/error.hpp"
#include "snabel/exec.hpp"
#include "snabel/func.hpp"
#include "snabel/op.hpp"
#include "snabel/proc.hpp"
#include "snackis/core/defer.hpp"

namespace snabel {
  OpImp::OpImp(OpCode code, const str &name):
    code(code), name(name)
  { }
  
  str OpImp::info() const {
    return "";
  }

  bool OpImp::prepare(Scope &scp) {
    return true;
  }

  bool OpImp::refresh(Scope &scp) {
    return false;
  }

  bool OpImp::compile(const Op &op, Scope &scp, OpSeq &out) {
    return false;
  }
  
  bool OpImp::finalize(const Op &op, Scope &scp, OpSeq &out) {
    return false;
  }

  bool OpImp::run(Scope &) {
    return true;
  }

  Backup::Backup(bool copy):
    OpImp(OP_BACKUP, "backup"), copy(copy)
  { }

  OpImp &Backup::get_imp(Op &op) const {
    return std::get<Backup>(op.data);
  }

  str Backup::info() const { return copy ? "copy" : ""; }

  bool Backup::run(Scope &scp) {
    backup_stack(scp.thread, copy);
    return true;
  }

  Break::Break(int64_t dep):
    OpImp(OP_BREAK, "break"), depth(dep)
  { }

  OpImp &Break::get_imp(Op &op) const {
    return std::get<Break>(op.data);
  }

  bool Break::run(Scope &scp) {
    return _break(scp.thread, depth);
  }
  
  Call::Call(opt<Box> target):
    OpImp(OP_CALL, "call"), target(target)
  { }

  OpImp &Call::get_imp(Op &op) const {
    return std::get<Call>(op.data);
  }

  bool Call::run(Scope &scp) {
    if (target) { return (*target->type->call)(scp, *target, false); }
    auto tgt(try_pop(scp.thread));
    
    if (!tgt) {
      ERROR(Snabel, "Missing call target");
      return false;
    }
    
    if (!tgt->type->call) {
      ERROR(Snabel, fmt("Invalid call target: %0", *tgt));
      return false;
    }
    
    bool ok((*tgt->type->call)(scp, *tgt, false));
    return ok;
  }

  Check::Check(Type *type):
    OpImp(OP_CHECK, "check"), type(type)
  { }

  OpImp &Check::get_imp(Op &op) const {
    return std::get<Check>(op.data);
  }

  str Check::info() const { return type ? type->name : ""; }

  bool Check::compile(const Op &op, Scope &scp, OpSeq &out) {
    if (out.empty()) { return false; }
    
    auto &prev(out.back());
    
    if (prev.imp.code == OP_PUSH) {
      auto &p(get<Push>(prev.data));
      auto &v(p.vals.back());
      
      if (isa(scp.thread, *v.type, scp.exec.meta_type)) {
	type = get<Type *>(v);
	p.vals.pop_back();
	if (p.vals.empty()) { out.pop_back(); }
	out.push_back(op);
	return true;
      }
    }

    return false;
  }

  
  bool Check::run(Scope &scp) {
    auto &exe(scp.exec);
    auto &thd(scp.thread);
    auto t(type);
    
    if (!t) {
      auto _t(try_pop(thd));
    
      if (!_t) {
	ERROR(Snabel, "Missing check type");
	return false;
      }
      
      if (!isa(thd, *_t, exe.meta_type)) {
	ERROR(Snabel, fmt("Invalid check type: %0", *_t));
	return false;
      }

      t = get<Type *>(*_t);
    }

    auto v(peek(thd));

    if (!v) {
      ERROR(Snabel, "Missing check value");
      return false;
    }
    
    if (!isa(thd, *v, *t)) {
      ERROR(Snabel, fmt("Check failed, expected %0!\n%1 %2!",
			t->name, *v, v->type->name));
      return false;
    }

    return true;
  }

  Deref::Deref(const str &name):
    OpImp(OP_DEREF, "deref"), name(name)
  { }

  OpImp &Deref::get_imp(Op &op) const {
    return std::get<Deref>(op.data);
  }

  str Deref::info() const { return name; }

  bool Deref::compile(const Op &op, Scope &scp, OpSeq &out) {
    auto &exe(scp.exec);
    auto fnd(find_env(scp, name));
    if (!fnd) { return false; }

    if (fnd->type == &exe.func_type) {
      out.emplace_back(Funcall(*get<Func *>(*fnd)));
    } else if (fnd->type == &exe.lambda_type) {
      out.emplace_back(Call(*fnd));
    } else if (fnd->type == &exe.label_type) {
      out.emplace_back(Jump(*get<Label *>(*fnd)));
    } else {
      out.emplace_back(Push(*fnd));
    }

    return true;
  }

  bool Deref::run(Scope &scp) {
    auto &exe(scp.exec);
    auto &thd(scp.thread);
    auto fnd(find_env(scp, name));
    
    if (!fnd) {
      ERROR(Snabel, fmt("Unknown identifier: %0", name));
      return false;
    }

    if (isa(thd, *fnd, exe.callable_type)) {
      return (*fnd->type->call)(scp, *fnd, false);
    }
    
    push(thd, *fnd);
    return true;
  }

  Drop::Drop(size_t count):
    OpImp(OP_DROP, "drop"), count(count)
  { }

  OpImp &Drop::get_imp(Op &op) const {
    return std::get<Drop>(op.data);
  }

  str Drop::info() const {
    return fmt_arg(count);
  }
  
  bool Drop::compile(const Op &op, Scope &scp, OpSeq &out) {
    bool done(false);
    size_t rem(0);

    while (!done && count > 0 && !out.empty()) {
      auto &o(out.back());
      
      switch(o.imp.code) {
      case OP_PUSH: {
	auto &p(get<Push>(o.data));
	while (count > 0 && !p.vals.empty()) {
	  count--;
	  rem++;
	  p.vals.pop_back();
	}

	if (p.vals.empty()) {
	  out.pop_back();
	} else {
	  done = true;
	}
	break;
      }
      case OP_DROP: {
	count += get<Drop>(o.data).count;
	rem++;
	out.pop_back();
	break;
      }
      default:
	done = true;
      }
    }
      
    if (rem) {
      if (count) { out.push_back(op); }
      return true;
    }
    
    return false;
  }
  
  bool Drop::run(Scope &scp) {
    auto &s(curr_stack(scp.thread));
    
    if (s.size() < count) {
      ERROR(Snabel, fmt("Not enough values on stack (%0):\n%1", count, s));
      return false;
    }
    
    for (size_t i(0); i < count; i++) { pop(scp.thread); }
    return true;
  }

  Dup::Dup():
    OpImp(OP_DUP, "dup")
  { }

  OpImp &Dup::get_imp(Op &op) const {
    return std::get<Dup>(op.data);
  }

  bool Dup::run(Scope &scp) {
    auto &s(curr_stack(scp.thread));
    if (s.empty()) {
      ERROR(Snabel, "Invalid dup");
      return false;
    }

    s.push_back(s.back());
    return true;
  }

  For::For():
    OpImp(OP_FOR, "for"), compiled(false)
  { }

  OpImp &For::get_imp(Op &op) const {
    return std::get<For>(op.data);
  }

  bool For::prepare(Scope &scp) {
    auto &exe(scp.exec);
    key = fmt_arg(uid(exe));
    return true;
  }
  
  bool For::compile(const Op &op, Scope &scp, OpSeq &out) {
    if (compiled) { return false; }
    compiled = true;
    auto &exe(scp.exec);
    auto &lbl(add_label(exe, fmt("_for%0", uid(exe))));
    out.emplace_back(Target(lbl));
    out.push_back(op);
    out.emplace_back(Jump(lbl));
    return true;
  }

  bool For::run(Scope &scp) {
    auto &exe(scp.exec);
    auto &thd(scp.thread);
    Iter::Ref it;
    opt<Box> tgt;
    auto fnd(find_env(scp, key));

    if (fnd) {
      auto &p(get<PairRef>(*fnd));
      it = get<Iter::Ref>(p->first);
      tgt.emplace(p->second);
    } else {    
      tgt = try_pop(thd);

      if (!tgt) {
	ERROR(Snabel, "Missing for target");
	return false;
      }
    
      if (!tgt->type->call) {
	ERROR(Snabel, fmt("Invalid for target: %0", *tgt));
	return false;
      }
    
      auto cnd(try_pop(thd));
    
      if (!cnd) {
	ERROR(Snabel, "Missing for condition");
	return false;
      }
        
      if (!cnd->type->iter)     {
	ERROR(Snabel, fmt("Invalid for condition: %0", *cnd));
	return false;
      }

      it = (*cnd->type->iter)(*cnd);
      put_env(scp, key, Box(exe.pair_type,
			    std::make_shared<Pair>(Box(exe.iter_type, it), *tgt)));
    }

    auto nxt(it->next(scp));
    
    if (nxt) {      
      push(thd, *nxt);
      scp.break_pc = thd.pc+2;
      (*tgt->type->call)(scp, *tgt, false);
    } else {
      rem_env(scp, key);
      scp.break_pc = -1;
      thd.pc += 2;
    }
    
    return true;
  }

  Funcall::Funcall(Func &fn):
    OpImp(OP_FUNCALL, "funcall"), fn(fn), imp(nullptr)
  { }

  OpImp &Funcall::get_imp(Op &op) const {
    return std::get<Funcall>(op.data);
  }

  str Funcall::info() const {
    return fn.name;
  }

  bool Funcall::run(Scope &scp) {
    auto &thd(scp.thread);
    
    if (imp) {
      auto m(match(*imp, thd, true));

      if (m) {
	(*imp)(scp, *m);
	return true;
      }
    }

    auto m(match(fn, thd));
    
    if (!m) {
      ERROR(Snabel, fmt("Function not applicable: %0\n%1", 
			fn.name, curr_stack(thd)));
      return false;
    }

    imp = m->first;
    (*imp)(scp, m->second);
    return true;
  }
  
  Getenv::Getenv(const str &id):
    OpImp(OP_GETENV, "getenv"), id(id)
  { }

  OpImp &Getenv::get_imp(Op &op) const {
    return std::get<Getenv>(op.data);
  }

  str Getenv::info() const { return id; }

  bool Getenv::refresh(Scope &scp) {
    if (val) { return false; }
    auto &exe(scp.exec);
    
    if (id == "return") {
      if (exe.lambdas.empty()) {
	ERROR(Snabel, "Missing return lambda");
	return false;
      }

      auto l(exe.lambdas.back());
      l->returns = true;

      if (l->exit_label) {
	val.emplace(exe.label_type, l->exit_label);
	return false;
      }
    } else if (id == "recall") {
      if (exe.lambdas.empty()) {
	ERROR(Snabel, "Missing recall lambda");
	return false;
      }

      auto l(exe.lambdas.back());
      l->recalls = true;

      if (l->recall_label) {
	val.emplace(exe.label_type, l->recall_label);
	return false;
      }      
    } else {
      return false;
    }

    return true;
  }
  
  bool Getenv::compile(const Op &op, Scope &scp, OpSeq &out) {
    if (val) {
      out.emplace_back(Push(*val));
      return true;
    }

    if (id.empty()) { return false; }
    
    auto fnd(find_env(scp, id));
    if (!fnd) { return false; }
    out.emplace_back(Push(*fnd));
    return true;
  }

  bool Getenv::run(Scope &scp) {
    auto &exe(scp.exec);
    auto &thd(scp.thread);
    str id_str(id);
    
    if (id_str.empty()) {
      auto id_arg(try_pop(thd));
      
      if (!id_arg) {
	ERROR(Snabel, "Missing identifier");
	return false;
      }
      
      if (id_arg->type != &exe.str_type) {
	ERROR(Snabel, fmt("Invalid identifier: %0", *id_arg));
	return false;
      }
      
      id_str = get<str>(*id_arg);
    }
    
    auto fnd(find_env(scp, id_str));

    if (!fnd) {
      ERROR(Snabel, fmt("Unknown identifier: %0", id_str));
      return false;
    }
    
    push(thd, *fnd);
    return true;
  }

  Jump::Jump(const str &tag):
    OpImp(OP_JUMP, "jump"), tag(tag), label(nullptr)
  { }

  Jump::Jump(Label &label):
    OpImp(OP_JUMP, "jump"), tag(label.tag), label(&label)
  { }

  OpImp &Jump::get_imp(Op &op) const {
    return std::get<Jump>(op.data);
  }

  str Jump::info() const {
    return fmt("%0:%1", tag, label ? to_str(label->pc) : "?");
  }

  bool Jump::refresh(Scope &scp) {
    if (!label) { label = find_label(scp.exec, tag); }
    return false;
  }

  bool Jump::run(Scope &scp) {
    if (!label) {
      ERROR(Snabel, fmt("Missing label: %0", tag));
      return false;
    }

    jump(scp, *label);
    return true;
  }
  
  Lambda::Lambda():
    OpImp(OP_LAMBDA, "lambda"),
    enter_label(nullptr),
    recall_label(nullptr),
    exit_label(nullptr),
    skip_label(nullptr),
    recalls(false), returns(false),
    compiled(false)
  { }

  OpImp &Lambda::get_imp(Op &op) const {
    return std::get<Lambda>(op.data);
  }

  bool Lambda::prepare(Scope &scp) {
    auto &exe(scp.exec);
    tag = uid(exe);
    enter_label = &add_label(exe, fmt("_enter%0", tag));
    skip_label = &add_label(exe, fmt("_skip%0", tag));    
    return true;
  }

  bool Lambda::refresh(Scope &scp) {
    auto &exe(scp.exec);
    exe.lambdas.push_back(this);
    bool changed(false);
    
    if (recalls && !recall_label) {
      recall_label = &add_label(exe, fmt("_recall%0", tag));
      recall_label->recall_target = true;
      changed = true;
    }

    return changed;
  }

  bool Lambda::compile(const Op &op, Scope &scp, OpSeq &out) {
    if (compiled) { return false; }
    compiled = true;
    out.emplace_back(Jump(*skip_label));
    out.emplace_back(Target(*enter_label));
    out.push_back(op);
    if (recall_label) { out.emplace_back(Target(*recall_label)); }
    return true;
  }

  bool Lambda::run(Scope &scp) {
    auto &thd(scp.thread);
    Scope &new_scp(begin_scope(thd, true));
    new_scp.target = enter_label;
    auto cor(find_coro(scp, *enter_label));

    if (cor) {
      if (cor->pc != -1) { thd.pc = cor->pc; }
      if (cor->proc) { new_scp.push_result = false; }
      std::copy(cor->stacks.begin(), cor->stacks.end(),
		std::back_inserter(thd.stacks));
      std::copy(cor->recalls.begin(), cor->recalls.end(),
		std::back_inserter(new_scp.recalls));
      for (auto &v: cor->env) { put_env(new_scp, v.first, v.second); }
    }

    return true;
  }

  Param::Param():
    OpImp(OP_PARAM, "param")
  { }

  OpImp &Param::get_imp(Op &op) const {
    return std::get<Param>(op.data);
  }
  
  Putenv::Putenv(const str &name):
    OpImp(OP_PUTENV, "putenv"), name(name)
  { }

  OpImp &Putenv::get_imp(Op &op) const {
    return std::get<Putenv>(op.data);
  }

  str Putenv::info() const { return name; }

  bool Putenv::prepare(Scope &scp) {
    auto fnd(find_env(scp, name));
      
    if (fnd) {
      ERROR(Snabel, fmt("Duplicate env: %0", name));
      return false;
    }

    return true;
  }
  
  bool Putenv::run(Scope &scp) {
    auto &s(curr_stack(scp.thread));

    if (s.empty()) {
      ERROR(Snabel, fmt("Missing env: %0", name));
      return false;
    }

    val.emplace(s.back());
    s.pop_back();
    put_env(scp, name, *val);
    return true;
  }

  Push::Push(const Box &val):
    OpImp(OP_PUSH, "push"), vals({val})
  { }

  Push::Push(const Stack &vals):
    OpImp(OP_PUSH, "push"), vals(vals)
  { }
  
  OpImp &Push::get_imp(Op &op) const {
    return std::get<Push>(op.data);
  }

  str Push::info() const { return fmt_arg(vals); }

  bool Push::compile(const Op &op, Scope &scp, OpSeq &out) {
    int popped(0);
    
    while (!out.empty()) {
      auto op(out.back());
      if (op.imp.code != OP_PUSH) { break; }
      auto vs(get<Push>(op.data).vals);
      std::copy(vs.rbegin(), vs.rend(), std::front_inserter(vals));
      out.pop_back();
      popped++;
    }

    if (!popped) { return false; }    
    out.push_back(op);
    return true;
  }
  
  bool Push::run(Scope &scp) {
    push(scp.thread, vals);
    return true;
  }

  Recall::Recall():
    OpImp(OP_RECALL, "recall"), label(nullptr)
  { }

  OpImp &Recall::get_imp(Op &op) const {
    return std::get<Recall>(op.data);
  }
  
  bool Recall::refresh(Scope &scp) {
    auto &exe(scp.exec);

    if (exe.lambdas.empty()) {
      ERROR(Snabel, "Missing lambda");
      return false;
    }
    
    auto l(exe.lambdas.back());
    
    if (l->recalls) {
      label = l->recall_label;
      return false;
    }

    l->recalls = true;
    return true;
  }

  bool Recall::compile(const Op &op, Scope &scp, OpSeq &out) {
    if (!label) {
      ERROR(Snabel, "Missing recall label");
      return false;
    }

    out.emplace_back(Jump(*label));
    return true;
  }
  
  Reset::Reset():
    OpImp(OP_RESET, "reset")
  { }

  OpImp &Reset::get_imp(Op &op) const {
    return std::get<Reset>(op.data);
  }

  bool Reset::finalize(const Op &op, Scope &scp, OpSeq &out) {
    for (auto i(out.rbegin()); i != out.rend(); i++) {
      if (i->imp.code == OP_BACKUP) {
	get<Backup>(i->data).copy = false;
      } else {
	break;
      }
    }

    return false;
  }
  
  bool Reset::run(Scope &scp) {
    curr_stack(scp.thread).clear();
    return true;
  }

  Restore::Restore():
    OpImp(OP_RESTORE, "restore")
  { }

  OpImp &Restore::get_imp(Op &op) const {
    return std::get<Restore>(op.data);
  }

  bool Restore::run(Scope &scp) {
    restore_stack(scp);
    return true;
  }

  Return::Return(bool scoped):
    OpImp(OP_RETURN, "return"), scoped(scoped), target(nullptr)
  { }

  OpImp &Return::get_imp(Op &op) const {
    return std::get<Return>(op.data);
  }

  bool Return::refresh(Scope &scp) {
    auto &exe(scp.exec);
    
    if (exe.lambdas.empty()) {
      ERROR(Snabel, "Missing lambda");
      return false;
    }
    
    auto l(exe.lambdas.back());

    if (!scoped && !l->returns) {
      l->returns = true;
      return true;
    }

    target = l->enter_label;
    return false;
  }

  bool Return::run(Scope &scp) {
    if (scp.recalls.empty()) {
      auto &thd(scp.thread);
      auto &prev_scp(*std::next(thd.scopes.rbegin()));

      if (!end_scope(thd)) { return false; }
      if (prev_scp.return_pc == -1) {
	ERROR(Snabel, "Missing return pc");
	return false;
      }

      thd.pc = prev_scp.return_pc;
      prev_scp.return_pc = -1;
      prev_scp.coros.erase(target);
    } else {
      recall_return(scp);
    }
    
    return true;
  }
  
  Stash::Stash():
    OpImp(OP_STASH, "stash")
  { }

  OpImp &Stash::get_imp(Op &op) const {
    return std::get<Stash>(op.data);
  }

  bool Stash::run(Scope &scp) {
    auto &exe(scp.exec);
    auto &thd(scp.thread);
    std::shared_ptr<List> lst(new List());
    lst->swap(curr_stack(thd));

    Type *elt(lst->empty() ? &exe.any_type : lst->at(0).type);

    if (!lst->empty()) {
      for (auto i(std::next(lst->begin())); i != lst->end() && elt; i++) {
	elt = get_super(*elt, *i->type);
      }
    }
    
    push(thd, Box(get_list_type(exe, elt ? *elt : exe.any_type), lst));
    return true;
  }

  Swap::Swap(size_t pos):
    OpImp(OP_SWAP, "swap"), pos(pos)
  { }

  OpImp &Swap::get_imp(Op &op) const {
    return std::get<Swap>(op.data);
  }

  str Swap::info() const { return fmt_arg(pos); }

  bool Swap::run(Scope &scp) {
    auto &s(curr_stack(scp.thread));
    if (s.size() < pos+1) {
      ERROR(Snabel, fmt("Invalid swap: %0\n%1", pos, s));
      return false;
    }

    auto i(s.size()-pos-1);
    auto v(s.at(i));
    s.erase(std::next(s.begin(), i));
    s.push_back(v);
    return true;
  }

  Target::Target(Label &label):
    OpImp(OP_TARGET, "target"), label(label)
  { }

  OpImp &Target::get_imp(Op &op) const {
    return std::get<Target>(op.data);
  }

  str Target::info() const {
    return fmt("%0:%1", label.tag, label.pc);
  }

  bool Target::finalize(const Op &op, Scope &scp, OpSeq & out) {
    label.pc = scp.thread.pc;    
    return true;
  }

  Unlambda::Unlambda():
    OpImp(OP_UNLAMBDA, "unlambda"),
    enter_label(nullptr), exit_label(nullptr), skip_label(nullptr),
    compiled(false)
  { }

  OpImp &Unlambda::get_imp(Op &op) const {
    return std::get<Unlambda>(op.data);
  }

  bool Unlambda::refresh(Scope &scp) {
    Exec &exe(scp.exec);
    
    if (exe.lambdas.empty()) {
      ERROR(Snabel, "Missing lambda");
      return false;
    }

    auto &l(*exe.lambdas.back());
    bool changed(false);
    
    if (!l.exit_label && (l.returns || l.recalls)) {
      exit_label = &add_label(exe, fmt("_exit%0", l.tag));
      l.exit_label = exit_label;
      changed = true;
    }

    enter_label = l.enter_label;
    skip_label = l.skip_label;
    exe.lambdas.pop_back();
    return changed;
  }
  
  bool Unlambda::compile(const Op &op, Scope &scp, OpSeq &out) {
    if (compiled) { return false; }  
    
    compiled = true;
    if (exit_label) { out.emplace_back(Target(*exit_label)); }
    out.emplace_back(Return(true));
    out.push_back(op);
    out.emplace_back(Target(*skip_label));
    out.emplace_back(Push(Box(scp.exec.lambda_type, enter_label)));
    return true;
  }

  bool Unlambda::finalize(const Op &op, Scope &scp, OpSeq &out) {
    return true;
  }
  
  Unparam::Unparam():
    OpImp(OP_UNPARAM, "unparam"), done(false)
  { }

  OpImp &Unparam::get_imp(Op &op) const {
    return std::get<Unparam>(op.data);
  }

  bool Unparam::compile(const Op &op, Scope &scp, OpSeq &out) {
    auto &exe(scp.exec);
    auto &thd(scp.thread);
    
    if (done) {
      if (out.empty()) { return false; }
      auto &prev(out.back());
      if (prev.imp.code == OP_PUSH) {
	auto &p(get<Push>(prev.data));
	auto &v(p.vals.back());
	
	if (isa(thd, *v.type, exe.meta_type)) {
	  auto &t(get_type(exe, *get<Type *>(v)->raw, types));
	  v.type = &get_meta_type(exe, t);
	  get<Type *>(v) = &t;
	  return true;
	}
      }
      
      return false;
    }
    
    auto i(out.rbegin());
    size_t cnt(0);
    
    for (; i != out.rend(); i++, cnt++) {
      if (i->imp.code == OP_PARAM) {
	cnt++;
	done = true;
	break;
      } else if (i->imp.code == OP_UNPARAM) {
	ERROR(Snabel, "Missing param start");
	return false;
      } else if (i->imp.code == OP_PUSH) {
	auto &p(get<Push>(i->data));

	while (!p.vals.empty()) {
	  if (!isa(thd, p.vals.back(), exe.meta_type)) { break; }
	  types.push_front(get<Type *>(p.vals.back()));
	  p.vals.pop_back();
	}

	if (!p.vals.empty()) {
	  break;
	}
      } else {
	break;
      }
    }

    if (cnt > 0) {
      while (cnt > 0) {
	out.pop_back();
	cnt--;
      }
      
      out.push_back(op);
      return true;
    }

    return false;
  }

  bool Unparam::run(Scope &scp) {
    if (!done) {
      ERROR(Snabel, "Failed parsing params");
      return false;
    }

    auto &exe(scp.exec);
    auto t(peek(scp.thread));

    if (!t) {
      ERROR(Snabel, "Missing param type");
    }
    
    if (!isa(scp.thread, *t->type, exe.meta_type)) {
      ERROR(Snabel, fmt("Invalid param type: %0", *t));
      return false;
    }

    auto &pt(get_type(exe, *get<Type *>(*t)->raw, types));
    t->type = &get_meta_type(exe, pt);
    get<Type *>(*t) = &pt;
    return true;
  }

  When::When(opt<Box> target):
    OpImp(OP_WHEN, "when"), target(target)
  { }

  OpImp &When::get_imp(Op &op) const {
    return std::get<When>(op.data);
  }

  bool When::prepare(Scope &scp) {
    if (target && !target->type->call) {
      ERROR(Snabel, fmt("Invalid when target: %0", *target));
      return false;
    }

    return true;
  }
  
  bool When::run(Scope &scp) {
    auto &exe(scp.exec);
    auto &thd(scp.thread);
    auto tgt(target);

    if (!tgt) {
      tgt = try_pop(thd);
      
      if (!tgt) {
	ERROR(Snabel, "Missing when target");
	return false;
      }
      
      if (!tgt->type->call) {
	ERROR(Snabel, fmt("Invalid when target: %0", *tgt));
	return false;
      }
    }

    auto cnd(try_pop(thd));

    if (!cnd) {
      ERROR(Snabel, "Missing when condition");
      return false;
    }

    if (cnd->type != &exe.bool_type) {
      ERROR(Snabel, fmt("Invalid when condition: %0", *cnd));
      return false;
    }

    if(get<bool>(*cnd)) { return (*tgt->type->call)(scp, *tgt, false); }
    return true;
  }

  While::While():
    OpImp(OP_WHILE, "while"), compiled(false)
  { }

  OpImp &While::get_imp(Op &op) const {
    return std::get<While>(op.data);
  }

  bool While::prepare(Scope &scp) {
    auto &exe(scp.exec);
    key = fmt_arg(uid(exe));
    return true;
  }

  bool While::compile(const Op &op, Scope &scp, OpSeq &out) {
    if (compiled) { return false; }
    compiled = true;
    auto &exe(scp.exec);
    auto &lbl(add_label(exe, fmt("_while%0", uid(exe))));
    out.emplace_back(Target(lbl));
    out.push_back(op);
    out.emplace_back(Jump(lbl));
    return true;
  }

  bool While::run(Scope &scp) {
    auto &exe(scp.exec);
    auto &thd(scp.thread);

    opt<Box> tgt, cnd;
    auto fnd(find_env(scp, key));

    if (fnd) {
      auto &p(get<PairRef>(*fnd));
      tgt.emplace(p->first);
      cnd.emplace(p->second);
    } else {
      tgt = try_pop(thd);
    
      if (!tgt) {
	ERROR(Snabel, "Missing while target");
	return false;
      }
      
      if (!tgt->type->call) {
	ERROR(Snabel, fmt("Invalid while target: %0", *tgt));
	return false;
      }
    
      cnd = try_pop(thd);
      
      if (!cnd) {
	ERROR(Snabel, "Missing while condition");
	return false;
      }
      
      if (!cnd->type->call)     {
	ERROR(Snabel, fmt("Invalid while condition: %0", *cnd));
	return false;
      }

      put_env(scp, key, Box(exe.pair_type,
			    std::make_shared<Pair>(*tgt, *cnd)));
    }
    
    (*cnd->type->call)(scp, *cnd, true);
    auto ok(try_pop(thd));

    if (ok) {
      if (ok->type != &exe.bool_type) {
	ERROR(Snabel, fmt("Invalid while condition: %0", *ok));
	return false;
      }
      
      if (get<bool>(*ok)) {
	scp.break_pc = scp.thread.pc+2;
	(*tgt->type->call)(scp, *tgt, false);
	return true;
      }
    } else {
      ERROR(Snabel, "Missing while condition");
      return false;
    }

    rem_env(scp, key);
    scp.break_pc = -1;
    scp.thread.pc += 2;
    return true;
  }

  Yield::Yield(int64_t depth):
    OpImp(OP_YIELD, "yield"), depth(depth)
  { }

  OpImp &Yield::get_imp(Op &op) const {
    return std::get<Yield>(op.data);
  }

  str Yield::info() const { return fmt_arg(depth); }

  bool Yield::run(Scope &scp) {
    return yield(scp, depth);
  }

  Op::Op(const Op &src):
    data(src.data), imp(src.imp.get_imp(*this)), prepared(src.prepared)
  { }

  bool prepare(Op &op, Scope &scp) {
    op.prepared = true;
    return op.imp.prepare(scp);
  }

  bool refresh(Op &op, Scope &scp) {
    return op.imp.refresh(scp);
  }

  bool compile(Op &op, Scope &scp, OpSeq &out) {
    if (op.imp.compile(op, scp, out)) { return true; }
    out.push_back(op);
    return false;
  }

  bool finalize(Op &op, Scope &scp, OpSeq &out) {
    if (op.imp.finalize(op, scp, out)) { return true; }
    out.push_back(op);
    return false;
  }
  
  bool run(Op &op, Scope &scp) {
    return op.imp.run(scp);
  }
}

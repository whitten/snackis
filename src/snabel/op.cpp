#include <algorithm>
#include <iostream>

#include "snabel/box.hpp"
#include "snabel/coro.hpp"
#include "snabel/error.hpp"
#include "snabel/exec.hpp"
#include "snabel/func.hpp"
#include "snabel/op.hpp"
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
    return true;
  }

  bool OpImp::compile(const Op &op, Scope &scp, OpSeq &out) {
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

  bool Backup::run(Scope &scp) {
    backup_stack(scp.coro, copy);
    return true;
  }
  
  Branch::Branch():
    OpImp(OP_BRANCH, "branch")
  { }

  OpImp &Branch::get_imp(Op &op) const {
    return std::get<Branch>(op.data);
  }

  bool Branch::run(Scope &scp) {
    auto &cor(scp.coro);
    auto &exe(cor.exec);    
    auto lbl(peek(cor));
    
    if (!lbl) {
      ERROR(Snabel, "Missing branch target");
      return false;
    }

    pop(cor);
    
    if (lbl->type != &exe.label_type) {
      ERROR(Snabel, fmt("Invalid branch target: %0", *lbl));
      return false;
    }

    auto cnd(peek(cor));

    if (!cnd) {
      ERROR(Snabel, "Missing branch condition");
      return false;
    }
    
    pop(cor);

    if (cnd->type != &cor.exec.bool_type) {
      ERROR(Snabel, fmt("Invalid branch condition: %0", *cnd));
      return false;
    }

    if(get<bool>(*cnd)) { call(scp, *get<Label *>(*lbl)); }
    return true;
  }
  
  Call::Call():
    OpImp(OP_CALL, "call"), label(nullptr)
  { }

  Call::Call(Label &label):
    OpImp(OP_CALL, "call"), label(&label)
  { }

  OpImp &Call::get_imp(Op &op) const {
    return std::get<Call>(op.data);
  }

  str Call::info() const {
      return label ? label->tag : "";    
  }

  bool Call::run(Scope &scp) {
    auto &cor(scp.coro);
    auto &exe(cor.exec);
      
    if (label) {
      call(scp, *label);
    } else {
      auto lbl(peek(cor));

      if (!lbl) {
	ERROR(Snabel, "Missing call target");
	return false;
      }
      
      pop(cor);

      if (lbl->type != &exe.label_type) {
	ERROR(Snabel, fmt("Invalid call target: %0", *lbl));
	return false;
      }
      
      call(scp, *get<Label *>(*lbl));
    }

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
    auto &s(curr_stack(scp.coro));
    if (s.size() < count) {
      ERROR(Snabel, fmt("Not enough values on stack (%0):\n%1", count, s));
      return false;
    }
    
    for (size_t i(0); i < count; i++) { pop(scp.coro); }
    return true;
  }

  Dup::Dup():
    OpImp(OP_DUP, "dup")
  { }

  OpImp &Dup::get_imp(Op &op) const {
    return std::get<Dup>(op.data);
  }

  bool Dup::run(Scope &scp) {
    auto &s(curr_stack(scp.coro));
    if (s.empty()) {
      ERROR(Snabel, "Invalid dup");
      return false;
    }

    s.push_back(s.back());
    return true;
  }

  Exit::Exit():
    OpImp(OP_EXIT, "exit"), label(nullptr)
  { }

  OpImp &Exit::get_imp(Op &op) const {
    return std::get<Exit>(op.data);
  }

  bool Exit::refresh(Scope &scp) {
    Coro &cor(scp.coro);
    Exec &exe(cor.exec);

    if (!label) {
      if (exe.lambdas.empty()) {
	ERROR(Snabel, "Missing lambda");
	return false;
      }

      str tag(exe.lambdas.back());
      auto lbl(find_label(exe, fmt("_exit%0", tag)));
      if (lbl) { label = lbl; }
    }

    return true;
  }
  
  bool Exit::compile(const Op &op, Scope &scp, OpSeq &out) {
    if (!label) {
      ERROR(Snabel, "Missing exit label");
      return false;
    }
    
    out.emplace_back(Jump(*label));
    return true;
  }

  bool Exit::run(Scope &scp) {
    ERROR(Snabel, "Missing exit label");
    return false;
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
    Coro &cor(scp.coro);
    if (imp && !match(*imp, cor)) { imp = nullptr; }
    if (!imp) { imp = match(fn, cor); }
    
    if (!imp) {
      ERROR(Snabel, fmt("Function not applicable: %0\n%1", 
			fn.name, curr_stack(scp.coro)));
      return false;
    }

    (*imp)(cor);
    return true;
  }

  Get::Get(const str &name):
    OpImp(OP_GET, "get"), name(name)
  { }

  OpImp &Get::get_imp(Op &op) const {
    return std::get<Get>(op.data);
  }

  str Get::info() const { return name; }

  bool Get::compile(const Op &op, Scope &scp, OpSeq &out) {
    Exec &exe(scp.coro.exec);
    auto fnd(find_env(scp, name));
    if (!fnd) { return false; }

    if (fnd->type == &exe.func_type && name.front() != '$') {
      out.emplace_back(Funcall(*get<Func *>(*fnd)));
    } else {
      out.emplace_back(Push(*fnd));
    }

    return true;
  }
  
  bool Get::run(Scope &scp) {
    Coro &cor(scp.coro);     
    auto fnd(find_env(scp, name));
    
    if (!fnd) {
      ERROR(Snabel, fmt("Unknown identifier: %0", name));
      return false;
    }
    
    push(cor, *fnd);
    val.emplace(*fnd);
    return true;
  }
  
  Group::Group(bool copy):
    OpImp(OP_GROUP, "group"), copy(copy)
  { }

  OpImp &Group::get_imp(Op &op) const {
    return std::get<Group>(op.data);
  }

  str Group::info() const { return copy ? "copy" : ""; }

  bool Group::refresh(Scope &scp) {
    return run(scp);
  }
  
  bool Group::run(Scope &scp) {
    begin_scope(scp.coro, copy);
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
    return fmt("%0 (%1)", tag, label ? to_str(label->pc) : "?");
  }

  bool Jump::refresh(Scope &scp) {
    auto &cor(scp.coro);
    auto &exe(cor.exec);
    if (!label) { label = find_label(exe, tag); }
    return true;
  }

  bool Jump::run(Scope &scp) {
    if (!label) {
      ERROR(Snabel, fmt("Missing label: %0", tag));
      return false;
    }

    jump(scp.coro, *label);
    return true;
  }
  
  Lambda::Lambda():
    OpImp(OP_LAMBDA, "lambda"), compiled(false)
  { }

  OpImp &Lambda::get_imp(Op &op) const {
    return std::get<Lambda>(op.data);
  }

  bool Lambda::prepare(Scope &scp) {
    tag = fmt_arg(gensym(scp.coro.exec));
    return true;
  }

  bool Lambda::refresh(Scope &scp) {
    scp.coro.exec.lambdas.push_back(tag);
    return run(scp);
  }

  bool Lambda::compile(const Op &op, Scope &scp, OpSeq &out) {
      if (compiled) { return false; }

      if (tag.empty()) {
	ERROR(Snabel, "Empty lambda tag");
	return false;
      }

      compiled = true;
      out.emplace_back(Jump(fmt("_skip%0", tag)));
      out.emplace_back(Target(fmt("_enter%0", tag)));
      out.emplace_back(Group(true));
      out.push_back(op);
      out.emplace_back(Target(fmt("_recall%0", tag)));
      return true;
  }

  Let::Let(const str &name):
    OpImp(OP_LET, "let"), name(name)
  { }

  OpImp &Let::get_imp(Op &op) const {
    return std::get<Let>(op.data);
  }

  str Let::info() const { return name; }

  bool Let::prepare(Scope &scp) {
    auto fnd(find_env(scp, name));
      
    if (fnd) {
      ERROR(Snabel, fmt("Duplicate binding: %0", name));
      return false;
    }

    return true;
  }
  
  bool Let::run(Scope &scp) {
    auto &s(curr_stack(scp.coro));

    if (s.empty()) {
      ERROR(Snabel, fmt("Missing bound val: %0", name));
      return false;
    }

    val.emplace(s.back());
    s.pop_back();
    put_env(scp, name, *val);
    return true;
  }

  Pointer::Pointer(const str &id):
    OpImp(OP_POINTER, "pointer"), id(id)
  { }

  OpImp &Pointer::get_imp(Op &op) const {
    return std::get<Pointer>(op.data);
  }

  bool Pointer::compile(const Op &op, Scope &scp, OpSeq &out) {
    Exec &exe(scp.coro.exec);
    auto fnd(find_env(scp, id));
    if (!fnd) { return false; }

    if (fnd->type == &exe.label_type) {
      out.emplace_back(Push(*fnd));
    } else if (fnd->type == &exe.func_type) {
      const Sym tag(gensym(exe));
      out.emplace_back(Jump(fmt("_skip%0", tag)));
      str call_lbl(fmt("_call%0", tag));
      out.emplace_back(Target(call_lbl));
      out.emplace_back(Funcall(*get<Func *>(*fnd)));
      out.emplace_back(Return(false));
      out.emplace_back(Target(fmt("_skip%0", tag)));
      out.emplace_back(Get(call_lbl));
    } else {
      return false;
    }

    return true;
  }

  bool Pointer::run(Scope &scp) {
    ERROR(Snabel, fmt("Missing pointer: %0", id));
    return false;
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

  bool Push::run(Scope &scp) {
    push(scp.coro, vals);
    return true;
  }

  Recall::Recall():
    OpImp(OP_RECALL, "recall"), label(nullptr)
  { }

  OpImp &Recall::get_imp(Op &op) const {
    return std::get<Recall>(op.data);
  }
  
  bool Recall::refresh(Scope &scp) {
    Coro &cor(scp.coro);
    Exec &exe(cor.exec);

    if (exe.lambdas.empty()) {
      ERROR(Snabel, "Missing lambda");
      return false;
    }
    
    auto tag = exe.lambdas.back();
    label = find_label(exe, fmt("_recall%0", tag));
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

  bool Recall::run(Scope &scp) {
    ERROR(Snabel, "Missing recall label");
    return false;
  }
  
  Reset::Reset():
    OpImp(OP_RESET, "reset")
  { }

  OpImp &Reset::get_imp(Op &op) const {
    return std::get<Reset>(op.data);
  }

  bool Reset::run(Scope &scp) {
    curr_stack(scp.coro).clear();
    return true;
  }

  Restore::Restore():
    OpImp(OP_RESTORE, "restore")
  { }

  OpImp &Restore::get_imp(Op &op) const {
    return std::get<Restore>(op.data);
  }

  bool Restore::run(Scope &scp) {
    restore_stack(scp.coro);
    return true;
  }

  Return::Return(bool scoped):
    OpImp(OP_RETURN, "return"), scoped(scoped)
  { }

  OpImp &Return::get_imp(Op &op) const {
    return std::get<Return>(op.data);
  }

  bool Return::refresh(Scope &scp) {
    if (scoped) { end_scope(scp.coro); }
    return true;
  }

  bool Return::run(Scope &scp) {
    Coro &cor(scp.coro);
    if (scoped && !end_scope(scp.coro)) { return false; }
    auto &prev_scp(curr_scope(cor));

    if (prev_scp.return_pc == -1) {
      ERROR(Snabel, "Missing return pc");
      return false;
    }

    cor.pc = prev_scp.return_pc;
    prev_scp.return_pc = -1;
    return true;
  }
  
  Stash::Stash():
    OpImp(OP_STASH, "stash")
  { }

  OpImp &Stash::get_imp(Op &op) const {
    return std::get<Stash>(op.data);
  }

  bool Stash::run(Scope &scp) {
    Coro &cor(scp.coro);
    Exec &exe(cor.exec);
    std::shared_ptr<List> lst(new List());
    lst->elems.swap(curr_stack(cor));

    Type *elt(lst->elems.empty() ? &exe.any_type : lst->elems[0].type);  
    for (auto i(std::next(lst->elems.begin())); i != lst->elems.end() && elt; i++) {
      elt = get_super(*elt, *i->type);
    }

    push(cor, Box(get_list_type(exe, elt ? *elt : exe.undef_type), lst));
    return true;
  }

  Swap::Swap():
    OpImp(OP_SWAP, "swap")
  { }

  OpImp &Swap::get_imp(Op &op) const {
    return std::get<Swap>(op.data);
  }

  bool Swap::run(Scope &scp) {
    auto &s(curr_stack(scp.coro));
    if (s.size() < 2) {
      ERROR(Snabel, fmt("Invalid swap:\n%0", s));
      return false;
    }
    
    auto x(s.back());
    s.pop_back();
    auto y(s.back());
    s.pop_back();
    s.push_back(x);
    s.push_back(y);
    return true;
  }

  Target::Target(const str &tag):
    OpImp(OP_TARGET, "target"), tag(tag), label(nullptr)
  { }

  OpImp &Target::get_imp(Op &op) const {
    return std::get<Target>(op.data);
  }

  str Target::info() const {
    if (!label) { return tag; }
    return fmt("%0 (%1:%2)", label->tag, label->depth, label->pc);
  }

  bool Target::prepare(Scope &scp) {
    auto &cor(scp.coro);
    auto &exe(cor.exec);
    auto fnd(find_label(exe, tag));

    if (fnd) {
      ERROR(Snabel, fmt("Duplicate label: %0", tag));
      return false;
    }
    
    label = &add_label(exe, tag);
    return true;
  }

  bool Target::refresh(Scope &scp) {
    if (!label) {
      ERROR(Snabel, fmt("Missing target label: %0", tag));
      return false;  
    }

    auto &cor(scp.coro);
    label->pc = cor.pc;
    label->depth = cor.scopes.size()+1;
    return true;
  }
  
  Ungroup::Ungroup():
    OpImp(OP_UNGROUP, "ungroup")
  { }

  OpImp &Ungroup::get_imp(Op &op) const {
    return std::get<Ungroup>(op.data);
  }

  bool Ungroup::refresh(Scope &scp) {
    return run(scp);
  }

  bool Ungroup::run(Scope &scp) {
    return end_scope(scp.coro);
  }

  Unlambda::Unlambda():
    OpImp(OP_UNLAMBDA, "unlambda"), compiled(false)
  { }

  OpImp &Unlambda::get_imp(Op &op) const {
    return std::get<Unlambda>(op.data);
  }

  bool Unlambda::refresh(Scope &scp) {
    Exec &exe(scp.coro.exec);
    
    if (exe.lambdas.empty()) {
      ERROR(Snabel, "Missing lambda");
      return false;
    }

    if (tag.empty()) {
      tag = exe.lambdas.back();
    } else if (tag != exe.lambdas.back()) {
      ERROR(Snabel, "Lambda tag changed");
      return false;
    }
    
    exe.lambdas.pop_back();
    return true;
  }
  
  bool Unlambda::compile(const Op &op, Scope &scp, OpSeq &out) {
    if (compiled || tag.empty()) { return false; }  
    
    compiled = true;
    out.emplace_back(Target(fmt("_exit%0", tag)));
    out.push_back(op);
    out.emplace_back(Return(true));
    out.emplace_back(Target(fmt("_skip%0", tag)));
    out.emplace_back(Get(fmt("_enter%0", tag)));
    return true;
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

  bool run(Op &op, Scope &scp) {
    return op.imp.run(scp);
  }
}

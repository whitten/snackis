#include <iostream>
#include "snabel/scope.hpp"
#include "snabel/error.hpp"
#include "snabel/exec.hpp"
#include "snackis/core/error.hpp"

namespace snabel {  
  Scope::Scope(Thread &thd):
    thread(thd),
    exec(thread.exec),
    target(nullptr),
    stack_depth(thread.stacks.size()),
    return_pc(-1),
    break_pc(-1),
    push_result(true)
  { }

  Scope::Scope(const Scope &src):
    thread(src.thread),
    exec(src.exec),
    target(nullptr),
    stack_depth(thread.stacks.size()),
    return_pc(-1),
    break_pc(-1),
    push_result(true),
    coros(src.coros)
  {}
  
  Scope::~Scope() {
    reset_stack(*this);
    rollback_env(*this);
  }
  
  void restore_stack(Scope &scp, size_t len) {
    auto &thd(scp.thread);
    CHECK(!thd.stacks.empty(), _);
    auto prev(thd.stacks.back());
    thd.stacks.pop_back();
    CHECK(!thd.stacks.empty(), _);

    if (len && ((thd.stacks.size() > scp.stack_depth) || scp.push_result)) {
      std::copy((prev.size() <= len)
		? prev.begin()
		: std::next(prev.begin(), prev.size()-len),
		prev.end(),
		std::back_inserter(curr_stack(thd)));
    }
  }

  Box *find_env(Scope &scp, const str &key) {
    auto &env(scp.thread.env);
    auto fnd(env.find(key));
    if (fnd == env.end()) { return nullptr; }
    return &fnd->second;
  }

  Box get_env(Scope &scp, const str &key) {
    auto fnd(find_env(scp, key));
    CHECK(fnd, _);
    return *fnd;
  }

  void put_env(Scope &scp, const str &key, const Box &val) {
    auto &env(scp.thread.env);
    auto fnd(env.find(key));

    if (fnd != env.end()) {
      ERROR(Snabel, fmt("'%0' is already bound to: %1", key, fnd->second));
    }

    if (fnd == env.end()) {
      env.emplace(std::piecewise_construct,
		  std::forward_as_tuple(key),
		  std::forward_as_tuple(val));
    } else {
      fnd->second = val;
    }

    if (&scp != &scp.thread.main_scope) { scp.env_keys.insert(key); }
  }

  bool rem_env(Scope &scp, const str &key) {
    if (&scp != &scp.thread.main_scope) {
      auto fnd(scp.env_keys.find(key));
      if (fnd == scp.env_keys.end()) { return false; }
      scp.env_keys.erase(fnd);
    }
    
    scp.thread.env.erase(key);
    return true;
  }

  void rollback_env(Scope &scp) {
    for (auto &k: scp.env_keys) { scp.thread.env.erase(k); }
    scp.env_keys.clear();
  }

  void reset_stack(Scope &scp) {
    reset_stack(scp.thread, scp.stack_depth, scp.push_result);
  }

  Coro &add_coro(Scope &scp, Label &tgt) {
    return *scp.coros.emplace(std::piecewise_construct,
			      std::forward_as_tuple(&tgt),
			      std::forward_as_tuple(std::make_shared<Coro>(scp)))
      .first->second;
  }

  Coro *find_coro(Scope &scp, Label &tgt) {
    auto fnd(scp.coros.find(&tgt));
    if (fnd == scp.coros.end()) { return nullptr; }
    return &*fnd->second;
  }

  void jump(Scope &scp, const Label &lbl) {
    auto &thd(scp.thread);

    if (lbl.yield_depth) {
	yield(scp, lbl.yield_depth);      
    } else if (lbl.break_depth) {
      _break(scp.thread, lbl.break_depth);
    } else {
      if (lbl.recall_target) {
	auto &frm(scp.recalls.emplace_back(scp));
	refresh(frm, scp);
	reset_stack(thd, scp.stack_depth, true);
      }
      
      thd.pc = lbl.pc;
    }
  }

  void call(Scope &scp, const Label &lbl, bool now) {
    CHECK(scp.return_pc == -1, _);
    scp.return_pc = scp.thread.pc+1;
    jump(scp, lbl);

    if (now) {
      auto ret_pc(scp.return_pc);
      run(scp.thread, scp.return_pc);
      if (scp.thread.pc == ret_pc) { scp.thread.pc--; }
    }
  }

  bool yield(Scope &scp, int64_t depth) {
    Thread &thd(scp.thread);
    bool dec_pc(false);
    
    while (depth && thd.scopes.size() > 1) {
      depth--;
      auto &prev_scp(*std::next(thd.scopes.rbegin()));

      if (!depth) {
	auto &curr_scp(thd.scopes.back());
	
	if (!curr_scp.target) {
	  ERROR(Snabel, "Missing yield target");
	  return false;
	}
	
	auto fnd(find_coro(prev_scp, *curr_scp.target));
	auto &cor(fnd ? *fnd : add_coro(prev_scp, *curr_scp.target));
	refresh(cor, curr_scp);
	if (dec_pc) { cor.pc--; }
	
	if (prev_scp.return_pc == -1) {
	  ERROR(Snabel, "Missing return pc");
	  return false;
	}	
      }

      dec_pc = true;
      thd.pc = prev_scp.return_pc;
      prev_scp.return_pc = -1;
      if (!end_scope(thd)) { return false; }
    }
    
    return true;
  }

  void recall_return(Scope &scp) {
    auto &thd(scp.thread);
    auto &frm(scp.recalls.back());
    thd.pc = frm.pc;
    opt<Box> last;
    if (!thd.stacks.back().empty()) { last = pop(thd); }    
    std::copy(frm.stacks.begin(), frm.stacks.end(),
	      std::back_inserter(thd.stacks));
    if (last) { push(thd, *last); }
    scp.recalls.pop_back();
  }

  Thread &start_thread(Scope &scp, const Box &init) {
    Thread &thd(scp.thread);
    Exec &exe(scp.exec);
    
    Thread::Id id(uid(exe));

    Thread *t(nullptr);

    {
      Exec::Lock lock(exe.mutex);
      t = &exe.threads.emplace(std::piecewise_construct,
			       std::forward_as_tuple(id),
			       std::forward_as_tuple(exe, id)).first->second;
    }
    
    auto &s(curr_stack(thd));
    std::copy(s.begin(), s.end(), std::back_inserter(curr_stack(*t)));

    auto &e(thd.env);
    auto &te(t->env);
    std::copy(e.begin(), e.end(), std::inserter(te, te.end()));

    std::copy(thd.ops.begin(), thd.ops.end(), std::back_inserter(t->ops));
    t->pc = t->ops.size();
    
    if ((*init.type->call)(curr_scope(*t), init, false)) {
      start(*t);
    } else {
      ERROR(Snabel, "Failed initializing thread");
    }
    
    return *t;
  }
}

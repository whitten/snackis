#ifndef SNACKIS_DB_CTX_HPP
#define SNACKIS_DB_CTX_HPP

#include <mutex>
#include <list>
#include <set>

#include "snackis/core/func.hpp"
#include "snackis/core/opt.hpp"
#include "snackis/core/path.hpp"
#include "snackis/core/str.hpp"
#include "snackis/crypt/secret.hpp"
#include "snackis/db/change.hpp"
#include "snackis/db/msg.hpp"
#include "snackis/db/proc.hpp"

namespace snackis {
namespace db {
  struct BasicTable;
  struct Trans;
  
  struct Ctx {
    using Lock = std::unique_lock<std::recursive_mutex>;
    
    Proc &proc;
    Chan<Msg> inbox;
    opt<crypt::Secret> secret;
    std::set<BasicTable *> tables;
    Trans *trans;
    std::list<ChangeSet> undo_stack;
    std::recursive_mutex mutex;
    
    Ctx(Proc &p, size_t max_buf);
    virtual ~Ctx();
  };

  Path get_path(const Ctx &ctx, const str &fname);
  Trans &get_trans(Ctx &ctx);
  void init_db_ver(Ctx &ctx);
  bool pass_exists(const Ctx &ctx);
  void init_pass(Ctx &ctx, const str &pass);
  bool login(Ctx &ctx, const str &pass);
  void open(Ctx &ctx);
  void slurp(Ctx &ctx);
  int64_t rewrite(Ctx &ctx);

  template <typename...Args>
  void log(const Ctx &ctx, const str &spec, const Args&...args) {
    log(ctx.proc, spec, std::forward<const Args &>(args)...);
  }
}}

#endif

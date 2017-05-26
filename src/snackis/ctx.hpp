#ifndef SNACKIS_CTX_HPP
#define SNACKIS_CTX_HPP

#include "snackis/db.hpp"
#include "snackis/db/ctx.hpp"
#include "snackis/settings.hpp"
#include "snackis/core/opt.hpp"
#include "snackis/core/str.hpp"

namespace snackis {
  struct Ctx: public db::Ctx {
    Db db;
    Settings settings;
    
    Ctx(const Path &path);
  };

  void log(const Ctx &ctx, const str &msg);
}

#endif

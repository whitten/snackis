#include <iostream>
#include "snackis/ctx.hpp"
#include "snackis/peer.hpp"
#include "snackis/project.hpp"

namespace snackis {
  Project::Project(Ctx &ctx): Rec(ctx), owner_id(whoami(ctx).id)
  { }

  Project::Project(Ctx &ctx, const db::Rec<Project> &rec): Rec(ctx), id(false) {
    copy(*this, rec);
  }

  Project::Project(const Msg &msg):
    Rec(msg.ctx),
    //id(msg.project_id),
    owner_id(msg.from_id)
    //name(msg.project_name),
    //info(msg.project_info)
  { }

  opt<Project> find_project_id(Ctx &ctx, UId id) {
    db::Rec<Project> rec;
    set(rec, ctx.db.project_id, id);
    if (!load(ctx.db.projects, rec)) { return nullopt; }
    return Project(ctx, rec);
  }

  Project get_project_id(Ctx &ctx, UId id) {
    auto found(find_project_id(ctx, id));
    
    if (!found) {
      ERROR(Db, fmt("Project id not found: %0", id));
    }

    return *found;
  }

  Feed get_feed(const Project &prj) {
    Ctx &ctx(prj.ctx);
    db::Rec<Feed> feed_rec;
    db::set(feed_rec, ctx.db.feed_id, prj.id);
    Feed feed(ctx, feed_rec);
    
    if (!db::load(ctx.db.feeds, feed)) {
      feed.name = fmt("Project %0", prj.name);
      feed.peer_ids = prj.peer_ids;
      db::upsert(ctx.db.feeds, feed);
    }

    return feed;
  }
}
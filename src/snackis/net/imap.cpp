#include <iostream>
#include <iterator>
#include "snackis/ctx.hpp"
#include "snackis/invite.hpp"
#include "snackis/core/fmt.hpp"
#include "snackis/core/stream.hpp"
#include "snackis/net/imap.hpp"

namespace snackis {
  ImapError::ImapError(const str &msg): Error(str("ImapError: ") + msg) { }

  static size_t on_read(char *ptr, size_t size, size_t nmemb, void *_out) {
    Stream *out = static_cast<Stream *>(_out);
    out->write(ptr, size * nmemb);
    return size * nmemb;  
  }

  static size_t skip_read(char *ptr, size_t size, size_t nmemb, void *_out) {
    return size * nmemb;  
  }

  Imap::Imap(Ctx &ctx): ctx(ctx), client(curl_easy_init()) {
    if (!client) { ERROR(Imap, "Failed initializing client"); }
    curl_easy_setopt(client, 
		     CURLOPT_USERNAME, 
		     get_val(ctx.settings.imap_user)->c_str());
    curl_easy_setopt(client, 
		     CURLOPT_PASSWORD, 
		     get_val(ctx.settings.imap_pass)->c_str());
    curl_easy_setopt(client,
		     CURLOPT_URL,
		     fmt("imaps://%0:%1/INBOX",
			 *get_val(ctx.settings.imap_url),
			 *get_val(ctx.settings.imap_port)).c_str());
    curl_easy_setopt(client, CURLOPT_WRITEFUNCTION, on_read);
    //curl_easy_setopt(client, CURLOPT_VERBOSE, 1L);

    log(ctx, "Connecting to Imap...");
    
    TRY(try_connect);
    noop(*this);
  }

  Imap::~Imap() { curl_easy_cleanup(client); }

  void noop(const struct Imap &imap) {
    curl_easy_setopt(imap.client, CURLOPT_CUSTOMREQUEST, "NOOP");
    curl_easy_setopt(imap.client, CURLOPT_HEADERFUNCTION, nullptr);
    curl_easy_setopt(imap.client, CURLOPT_WRITEFUNCTION, skip_read);
    CURLcode res(curl_easy_perform(imap.client));
 
    if (res != CURLE_OK) {
      ERROR(Imap, fmt("Failed sending NOOP: %0", curl_easy_strerror(res))); 
    }
  }

  static void delete_uid(const struct Imap &imap, const str &uid) {
    curl_easy_setopt(imap.client,
		     CURLOPT_CUSTOMREQUEST,
		     fmt("UID STORE %0 +FLAGS.SILENT \\Deleted", uid).c_str());

    curl_easy_setopt(imap.client, CURLOPT_HEADERFUNCTION, nullptr);
    curl_easy_setopt(imap.client, CURLOPT_WRITEFUNCTION, skip_read);
    CURLcode res(curl_easy_perform(imap.client));
 
    if (res != CURLE_OK) {
      ERROR(Imap, fmt("Failed deleting uid: %0", curl_easy_strerror(res))); 
    }
  }

  static void expunge(const struct Imap &imap) {
    curl_easy_setopt(imap.client, CURLOPT_CUSTOMREQUEST, "EXPUNGE");

    curl_easy_setopt(imap.client, CURLOPT_HEADERFUNCTION, nullptr);
    curl_easy_setopt(imap.client, CURLOPT_WRITEFUNCTION, skip_read);
    CURLcode res(curl_easy_perform(imap.client));
 
    if (res != CURLE_OK) {
      ERROR(Imap, fmt("Failed expunging: %0", curl_easy_strerror(res)));
    }
  }

  static opt<Msg> fetch_uid(const struct Imap &imap, const str &uid) {
    curl_easy_setopt(imap.client,
		     CURLOPT_CUSTOMREQUEST,
		     fmt("UID FETCH %0 BODY[TEXT]", uid).c_str());

    Stream out;    
    curl_easy_setopt(imap.client, CURLOPT_HEADERFUNCTION, on_read);
    curl_easy_setopt(imap.client, CURLOPT_HEADERDATA, &out);
    CURLcode res(curl_easy_perform(imap.client));
 
    if (res != CURLE_OK) {
      ERROR(Imap, fmt("Failed fetching uid: %0", curl_easy_strerror(res)));
    }

    db::Rec<Msg> rec;
    Msg msg(imap.ctx, rec);
    const str body(out.str());
    const str tag("__SNACKIS__\r\n");
    auto i(body.find(tag) + tag.size());
    if (i == str::npos || !decode(msg, body.substr(i))) { return nullopt; }
    return msg;
  }

  static void handle_post_msg(Imap &imap, const Msg &msg) {
    Ctx &ctx(imap.ctx);
    opt<Feed> fd_fnd(find_feed_id(ctx, *db::get(msg.feed, ctx.db.feed_id)));
    
    if (fd_fnd) {
      opt<Post> ps_fnd(find_post_id(ctx, *db::get(msg.post, ctx.db.post_id)));
      Peer pr(get_peer_id(ctx, msg.from_id));

      if (fd_fnd->owner_id == pr.id) {
	copy(*fd_fnd, msg);
	db::update(ctx.db.feeds, *fd_fnd);
      }

      if (ps_fnd) {
	if (ps_fnd->owner_id == pr.id) {
	  copy(*ps_fnd, msg);
	  db::update(ctx.db.posts, *ps_fnd);
	  log(ctx, fmt("Post %0 updated:\n%1", id_str(*ps_fnd), ps_fnd->body));
	} else {
	  log(ctx, fmt("Unautorized post update from %0", msg.from));
	}
      } else {
	Post ps(msg);
	db::insert(ctx.db.posts, ps);
	log(ctx, fmt("New post %0 by %1:\n%2", id_str(ps), pr.name, ps.body));
      }
    } else {
      const UId fd_id(*db::get(msg.feed, ctx.db.feed_id));
      
      auto prj(find_project_id(ctx, fd_id));
      auto tsk(find_task_id(ctx, fd_id));
      auto ps(find_post_id(ctx, fd_id));
      
      if ((prj && msg.from_id == prj->owner_id) ||
	  (tsk && msg.from_id == tsk->owner_id) ||
	  (ps && msg.from_id == ps->owner_id)) {
	Feed fd(msg);
	db::insert(ctx.db.feeds, fd);
	log(ctx, fmt("New feed: %0\n%1", id_str(fd), fd.name));
      } else if (prj || tsk || ps) {
	  log(ctx, fmt("Unautorized feed from %0", msg.from));	
      } else {
	db::insert(ctx.db.inbox, msg);
      }
    }
  }

  static void handle_task_msg(Imap &imap, const Msg &msg) {
    Ctx &ctx(imap.ctx);
    auto prj_fnd(find_project_id(ctx, *db::get(msg.project, ctx.db.project_id)));
    
    if (prj_fnd) {
      opt<Task> tsk_fnd(find_task_id(ctx, *db::get(msg.task, ctx.db.task_id)));
      Peer pr(get_peer_id(ctx, msg.from_id));

      if (prj_fnd->owner_id == pr.id) {
	copy(*prj_fnd, msg);
	db::update(ctx.db.projects, *prj_fnd);
      }

      if (tsk_fnd) {
	if (tsk_fnd->owner_id == pr.id) {
	  copy(*tsk_fnd, msg);
	  db::update(ctx.db.tasks, *tsk_fnd);
	  log(ctx, fmt("Task %0 updated", id_str(*tsk_fnd)));
	} else {
	  log(ctx, fmt("Unautorized task update from %0", msg.from));
	}
      } else {
	Task tsk(msg);
	db::insert(ctx.db.tasks, tsk);	
	log(ctx, fmt("New task %0:\n%1\n%2",
		     id_str(tsk),
		     tsk.name,
		     tsk.info));
      }
    } else {
      db::insert(ctx.db.inbox, msg);
    }
  }

  static void handle_msg(Imap &imap, const Msg &msg) {
    Ctx &ctx(imap.ctx);

    if (msg.type == Msg::ACCEPT || msg.type == Msg::REJECT) {
      db::Rec<Invite> inv;
      set(inv, ctx.db.invite_to, msg.from);
      
      if (!load(ctx.db.invites, inv)) {
	log(ctx, fmt("Missing invite: %0", msg.from));
	return;
      }
    }
    
    if (msg.type == Msg::REJECT) {
      invite_rejected(msg);
      log(ctx, fmt("Invite to %0 was rejected", msg.from));
    } else if (msg.type == Msg::POST) {
      handle_post_msg(imap, msg);
    } else if (msg.type == Msg::TASK) {
      handle_task_msg(imap, msg);
    } else {
      db::insert(ctx.db.inbox, msg);
    }
  }
  
  void fetch(struct Imap &imap) {
    TRACE("Fetching email");
    Ctx &ctx(imap.ctx);
    
    log(ctx, "Fetching email...");
    curl_easy_setopt(imap.client,
		     CURLOPT_CUSTOMREQUEST,
		     "UID SEARCH Subject \"__SNACKIS__\"");

    Stream out;    
    curl_easy_setopt(imap.client, CURLOPT_HEADERFUNCTION, nullptr);
    curl_easy_setopt(imap.client, CURLOPT_HEADERDATA, nullptr);
    curl_easy_setopt(imap.client, CURLOPT_WRITEFUNCTION, on_read);
    curl_easy_setopt(imap.client, CURLOPT_WRITEDATA, &out);
    CURLcode res(curl_easy_perform(imap.client));
 
    if (res != CURLE_OK) {
      ERROR(Imap, fmt("Failed searching inbox: %0", curl_easy_strerror(res)));
    }

    std::vector<str> tokens {
      std::istream_iterator<str>{out}, std::istream_iterator<str>{}
    };

    if (tokens.size() < 2 || tokens[1] != "SEARCH") {
      ERROR(Imap, fmt("Invalid fetch result:\n%0", out.str())); 
    }

    int msg_cnt = 0;
    
    if (tokens.size() > 2) {
      for (auto tok = std::next(tokens.begin(), 2); tok != tokens.end(); tok++) {
	db::Trans trans(ctx);
	TRY(try_msg);
	
	const str uid(*tok);
	opt<Msg> msg = fetch_uid(imap, uid);
	
	if (msg) {
	  handle_msg(imap, *msg);
	  msg_cnt++;
	} else {
	  log(ctx, "Failed decoding message");
	}
	
	if (try_msg.errors.empty()) {
	  delete_uid(imap, uid);
	  db::commit(trans);
	}
      }

      expunge(imap);
    }

    log(ctx, fmt("Finished fetching %0 messages", msg_cnt));
  }
}

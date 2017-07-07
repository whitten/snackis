#include <cassert>
#include "snackis/ctx.hpp"
#include "snackis/gui/gui.hpp"
#include "snackis/gui/feed_view.hpp"
#include "snackis/gui/peer_search.hpp"
#include "snackis/gui/post_search.hpp"
#include "snackis/gui/post_view.hpp"

namespace snackis {
namespace gui {
  static void on_find_posts(gpointer *_, FeedView *v) {
    PostSearch *ps = new PostSearch(v->ctx);
    select<Feed>(ps->feed_fld, v->rec);
    push_view(*ps);
  }

  static void on_post(gpointer *_, FeedView *v) {
    db::Trans trans(v->ctx);
    TRY(try_save);
    v->save();

    if (try_save.errors.empty()) {
      db::commit(trans);
      Post post(v->ctx);
      set_feed(post, v->rec);
      PostView *pv = new PostView(post);
      push_view(*pv);
    }
  }
  
  FeedView::FeedView(const Feed &feed):
    RecView("Feed", feed),
    find_posts_btn(gtk_button_new_with_mnemonic("_Find Posts")),
    post_btn(gtk_button_new_with_mnemonic("New _Post")),
    name_fld(gtk_entry_new()),
    tags_fld(gtk_entry_new()),
    active_fld(gtk_check_button_new_with_label("Active")),
    info_fld(new_text_view()),
    peer_lst(ctx, "Peer", this->rec.peer_ids)
  {
    GtkWidget *lbl;

    GtkWidget *btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_margin_bottom(btns, 5);
    gtk_container_add(GTK_CONTAINER(fields), btns);
    g_signal_connect(find_posts_btn, "clicked", G_CALLBACK(on_find_posts), this);
    gtk_container_add(GTK_CONTAINER(btns), find_posts_btn);
    g_signal_connect(post_btn, "clicked", G_CALLBACK(on_post), this);
    gtk_container_add(GTK_CONTAINER(btns), post_btn);

    lbl = gtk_label_new("Name");
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_container_add(GTK_CONTAINER(fields), lbl);
    GtkWidget *name_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_add(GTK_CONTAINER(fields), name_box);
    gtk_widget_set_hexpand(name_fld, true);
    gtk_container_add(GTK_CONTAINER(name_box), name_fld);    
    gtk_entry_set_text(GTK_ENTRY(name_fld), rec.name.c_str());
    gtk_container_add(GTK_CONTAINER(name_box), active_fld);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(active_fld), rec.active);

    lbl = gtk_label_new("Tags");
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_container_add(GTK_CONTAINER(fields), lbl);
    gtk_widget_set_hexpand(tags_fld, true);
    gtk_container_add(GTK_CONTAINER(fields), tags_fld);    
    gtk_entry_set_text(GTK_ENTRY(tags_fld),
		       join(rec.tags.begin(), rec.tags.end(), ' ').c_str());

    lbl = gtk_label_new("Info");
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_widget_set_margin_top(lbl, 5);
    gtk_container_add(GTK_CONTAINER(fields), lbl);
    gtk_container_add(GTK_CONTAINER(fields), gtk_widget_get_parent(info_fld));
    set_str(GTK_TEXT_VIEW(info_fld), rec.info);

    gtk_widget_set_margin_top(peer_lst.ptr(), 5);
    gtk_container_add(GTK_CONTAINER(fields), peer_lst.ptr());

    focused = name_fld;
    load(peer_lst);
    refresh(*this);
  }

  bool FeedView::save() {
    rec.name = gtk_entry_get_text(GTK_ENTRY(name_fld));
    rec.tags = word_set(get_str(GTK_ENTRY(tags_fld)));
    rec.info = get_str(GTK_TEXT_VIEW(info_fld));
    rec.active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(active_fld));
    db::upsert(ctx.db.feeds, rec);
    return true;
  }

}}

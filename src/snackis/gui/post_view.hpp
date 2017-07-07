#ifndef SNACKIS_GUI_POST_VIEW_HPP
#define SNACKIS_GUI_POST_VIEW_HPP

#include "snackis/post.hpp"
#include "snackis/gui/feed_select.hpp"
#include "snackis/gui/rec_view.hpp"

namespace snackis {
namespace gui {
  struct PostView: RecView<Post> {
    GtkListStore *post_store;
    GtkWidget *find_replies_btn, *reply_btn, *edit_feed_btn, *tags_fld, *body_fld,
      *post_lst;
    FeedSelect feed_fld;
    
    PostView(const Post &post);
    bool allow_save() const override;
    bool save() override;
  };
}}

#endif

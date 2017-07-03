#ifndef SNACKIS_GUI_FEED_VIEW_HPP
#define SNACKIS_GUI_FEED_VIEW_HPP

#include <mutex>
#include <set>
#include <vector>

#include "snackis/feed.hpp"
#include "snackis/gui/rec_view.hpp"

namespace snackis {
namespace gui {
  struct FeedView: RecView<Feed> {
    GtkListStore *peer_store;
    GtkWidget *find_posts_btn, *post_btn, *name_fld, *active_fld, *info_fld,
      *peer_lst, *add_peer_btn;
    
    FeedView(const Feed &feed);
    void init() override;
    bool allow_save() const override;
    bool save() override;
  };
}}

#endif

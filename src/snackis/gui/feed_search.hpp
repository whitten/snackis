#ifndef SNACKIS_GUI_FEED_SEARCH_HPP
#define SNACKIS_GUI_FEED_SEARCH_HPP

#include "snackis/feed.hpp"
#include "snackis/gui/peer_select.hpp"
#include "snackis/gui/search_view.hpp"

namespace snackis {
namespace gui {
  struct FeedSearch: SearchView<Feed> {
    GtkWidget *id_fld, *active_fld, *tags_fld, *text_fld;
    PeerSelect peer_fld;

    FeedSearch(Ctx &ctx);
    void find() override;    
  };
}}

#endif

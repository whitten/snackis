#ifndef SNACKIS_GUI_FEED_SEARCH_HPP
#define SNACKIS_GUI_FEED_SEARCH_HPP

#include <mutex>
#include <set>
#include <vector>

#include "snackis/gui/view.hpp"

namespace snackis {
namespace gui {
  struct FeedSearch: View {
    GtkListStore *peers, *feeds;
    GtkWidget *peer, *active, *find, *list, *edit, *close;
    
    FeedSearch(Ctx &ctx);
  };
}}

#endif

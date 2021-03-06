#ifndef SNACKIS_GUI_PEER_SEARCH_HPP
#define SNACKIS_GUI_PEER_SEARCH_HPP

#include "snackis/peer.hpp"
#include "snackis/gui/search_view.hpp"

namespace snackis {
namespace gui {
  struct PeerSearch: SearchView<Peer> {
    GtkWidget *id_fld, *active_fld, *tags_fld, *text_fld;
    
    PeerSearch(Ctx &ctx);
    void find() override;
  };
}}

#endif

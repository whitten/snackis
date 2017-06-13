#include <cassert>
#include "snackis/core/fmt.hpp"
#include "snackis/core/time.hpp"
#include "snackis/gui/gui.hpp"
#include "snackis/gui/console.hpp"
#include "snackis/gui/widget.hpp"

namespace snackis {
namespace gui {
  static void refresh(Console &cns) {
    Console::LogLock lock(cns.log_mutex);

    for (const auto &msg: cns.out) {
      auto buf(gtk_text_view_get_buffer(GTK_TEXT_VIEW(cns.text_view)));
      GtkTextIter end;
      gtk_text_buffer_get_end_iter(buf, &end);
      gtk_text_buffer_place_cursor(buf, &end);
      gtk_text_buffer_insert_at_cursor(buf, msg.c_str(), msg.size());
    }

    cns.out.clear();
  }

  static gboolean on_idle(gpointer cns) {
    refresh(*static_cast<Console *>(cns));
    return false;
  }
  
  void log(Console &cns, const str msg) {
    const str m(fmt("%0 %1\n", fmt(now(), "%a %H:%M:%S"), msg));
    Console::LogLock lock(cns.log_mutex);
    cns.out.push_back(m);
    g_idle_add(on_idle, &cns);
  }

  Console::Console():
    text_view(gtk_text_view_new()), scroll_view(gtk_scrolled_window_new(NULL, NULL)) {
    add_style(text_view, "console");
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), false);
    gtk_scrolled_window_set_overlay_scrolling(GTK_SCROLLED_WINDOW(scroll_view),
					      false);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_view),
				   GTK_POLICY_NEVER,
				   GTK_POLICY_ALWAYS);
    gtk_container_add(GTK_CONTAINER(scroll_view), text_view);
  }

  GtkWidget *Console::ptr() {
    return scroll_view;
  }
}}

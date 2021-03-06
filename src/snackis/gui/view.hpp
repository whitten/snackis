#ifndef SNACKIS_GUI_VIEW_HPP
#define SNACKIS_GUI_VIEW_HPP

#include <list>
#include "snackis/gui/widget.hpp"

namespace snackis {
  struct Ctx;
namespace gui {
  struct View: Widget {
    static std::list<View *> stack;
    Ctx &ctx;
    GtkWidget *panel, *label, *menu, *focused;
    bool visible;
    
    View(Ctx &ctx, const str &lbl);
    virtual ~View();
    virtual void load();
    GtkWidget *ptr() override;
  };

  void push_view(View *v);
  void pop_view(View *v);
  void switch_view();
}}


#endif

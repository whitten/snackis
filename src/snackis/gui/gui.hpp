#ifndef SNACKIS_GUI_HPP
#define SNACKIS_GUI_HPP

#include <gtk/gtk.h>
#include "snackis/core/opt.hpp"
#include "snackis/core/str.hpp"
#include "snackis/db/rec.hpp"
#include "snackis/gui/console.hpp"
#include "snackis/gui/inbox.hpp"
#include "snackis/gui/login.hpp"
#include "snackis/gui/reader.hpp"
#include "snackis/gui/root_view.hpp"
#include "snackis/gui/setup.hpp"
#include "snackis/gui/todo.hpp"
#include "snackis/gui/undo.hpp"

namespace snackis {
namespace gui {
  const int ID_LEN(8);
  
  extern GtkWidget *window, *panels, *main_panel, *left_panel;
  
  extern opt<Console> console;
  extern opt<Reader> reader;
  extern std::unique_ptr<RootView> root_view;
  extern std::unique_ptr<Setup> setup;
  extern std::unique_ptr<Inbox> inbox;
  extern std::unique_ptr<Todo> todo;
  extern std::unique_ptr<Undo> undo;
  
  str get_str(GtkEntry *w);
  str get_str(GtkTextView *w);
  void set_str(GtkEntry *w, const str &in);
  void set_str(GtkTextView *w, const str &in);

  template <typename RecT>
  const db::Rec<RecT> *get_sel_rec(GtkComboBox *w) {
    GtkTreeIter iter;
    if (!gtk_combo_box_get_active_iter(w, &iter)) { return nullptr; }
    const db::Rec<RecT> *rec(nullptr);
    GtkTreeModel *mod(gtk_combo_box_get_model(w));
    gtk_tree_model_get(mod, &iter, 0, &rec, -1); 
    return rec;
  }

  opt<GtkTreeIter> get_sel_iter(GtkTreeView *w);

  template <typename RecT>
  const db::Rec<RecT> *get_rec(GtkTreeView *w, GtkTreeIter &it) {
    const db::Rec<RecT> *rec(nullptr);
    gtk_tree_model_get(gtk_tree_view_get_model(w), &it, 0, &rec, -1);
    return rec;
  }

  template <typename RecT>
  const db::Rec<RecT> *get_sel_rec(GtkTreeView *w) {
    auto iter(get_sel_iter(w));
    if (!iter) { return nullptr; }
    return get_rec<RecT>(w, *iter);
  }

  void each_sel(GtkTreeView *w, func<void (GtkTreeIter &)> fn);
  void enable_multi_sel(GtkTreeView *w);
  bool sel_first(GtkTreeView *w);
  int sel_count(GtkTreeView *w);

  void read_only(GtkEntry *w);
  void read_only(GtkTextView *w);
  
  GtkWidget *new_grid();
  GtkWidget *new_label(const str &txt);
  GtkWidget *new_hint(const str &txt);
  void set_width(GtkEntry *e, int w);
  GtkWidget *new_id_field();
  GtkWidget *new_combo_box(GtkTreeModel *mod);
  GtkWidget *new_text_view();
  GtkWidget *new_tree_view(GtkTreeModel *mod);

  std::pair<GtkTreeViewColumn *, GtkCellRenderer *>
  add_col(GtkTreeView *w, const str &lbl, int idx,
			     bool exp=false);

  std::pair<GtkTreeViewColumn *, GtkCellRenderer *>
  add_check_col(GtkTreeView *w, const str &lbl, int idx);
}}

#endif

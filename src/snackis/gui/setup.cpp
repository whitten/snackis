#include "snackis/ctx.hpp"
#include "snackis/gui/gui.hpp"
#include "snackis/gui/setup.hpp"
#include "snackis/net/imap.hpp"
#include "snackis/net/smtp.hpp"

namespace snackis {
namespace gui {
  static void on_load_folder(gpointer *_, Setup *v) {
    GtkWidget *dlg(gtk_file_chooser_dialog_new("Select Load Folder",
					       GTK_WINDOW(window),
					       GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER,
					       "_Cancel", GTK_RESPONSE_CANCEL,
					       "_Select", GTK_RESPONSE_ACCEPT,
					       nullptr));
    const char *f(gtk_entry_get_text(GTK_ENTRY(v->load_folder)));
    gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dlg), f);

    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
      char *dir = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
      gtk_entry_set_text(GTK_ENTRY(v->load_folder), dir);
      g_free(dir);
    }

    gtk_widget_destroy(dlg);
  }

  static void on_save_folder(gpointer *_, Setup *v) {
    GtkWidget *dlg(gtk_file_chooser_dialog_new("Select Save Folder",
					       GTK_WINDOW(window),
					       GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER,
					       "_Cancel", GTK_RESPONSE_CANCEL,
					       "_Select", GTK_RESPONSE_ACCEPT,
					       nullptr));
    const char *f(gtk_entry_get_text(GTK_ENTRY(v->save_folder)));
    gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dlg), f);

    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
      char *dir = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
      gtk_entry_set_text(GTK_ENTRY(v->save_folder), dir);
      g_free(dir);
    }

    gtk_widget_destroy(dlg);
  }

  static void on_cancel(gpointer *_, Setup *v) {
    log(v->ctx, "Cancelled setup");
    pop_view(v);
  }

  static void copy_imap(Setup &v) {
    Ctx &ctx(v.ctx);
    set_val(ctx.settings.imap_url,
	    str(gtk_entry_get_text(GTK_ENTRY(v.imap_url))));

    const str port_str(gtk_entry_get_text(GTK_ENTRY(v.imap_port)));
    auto port(to_int64(port_str));
    
    if (!port) {
      log(ctx, fmt("Invalid Imap port: %0", port_str));
    } else {
      set_val(ctx.settings.imap_port, port);
    }

    set_val(ctx.settings.imap_user,
	    str(gtk_entry_get_text(GTK_ENTRY(v.imap_user))));
    set_val(ctx.settings.imap_pass,
	    str(gtk_entry_get_text(GTK_ENTRY(v.imap_pass))));

    const str poll_str(gtk_entry_get_text(GTK_ENTRY(v.imap_poll)));
    set_val(ctx.settings.imap_poll, to_int64(poll_str));
  }

  static void copy_smtp(Setup &v) {
    Ctx &ctx(v.ctx);
    set_val(ctx.settings.smtp_url,
	    str(gtk_entry_get_text(GTK_ENTRY(v.smtp_url))));

    const str port_str(gtk_entry_get_text(GTK_ENTRY(v.smtp_port)));
    auto port(to_int64(port_str));
    
    if (!port) {
      log(ctx, fmt("Invalid Smtp port: %0", port_str));
    } else {
      set_val(ctx.settings.smtp_port, port);
    }

    set_val(ctx.settings.smtp_user,
	    str(gtk_entry_get_text(GTK_ENTRY(v.smtp_user))));
    set_val(ctx.settings.smtp_pass,
	    str(gtk_entry_get_text(GTK_ENTRY(v.smtp_pass))));

    const str poll_str(gtk_entry_get_text(GTK_ENTRY(v.smtp_poll)));
    set_val(ctx.settings.smtp_poll, to_int64(poll_str));
  }

  static void on_save(gpointer *_, Setup *v) {
    Ctx &ctx(v->ctx);
    db::Trans trans(ctx);
    TRY(try_save);
    
    Peer &me(whoami(ctx));
    me.name = gtk_entry_get_text(GTK_ENTRY(v->name));
    me.email = gtk_entry_get_text(GTK_ENTRY(v->email));
    update(ctx.db.peers, me);

    set_val(ctx.settings.load_folder,
	    str(gtk_entry_get_text(GTK_ENTRY(v->load_folder))));
    set_val(ctx.settings.save_folder,
	    str(gtk_entry_get_text(GTK_ENTRY(v->save_folder))));

    copy_imap(*v);
    if (*get_val(ctx.settings.imap_poll)) {
      ctx.fetch_cond.notify_one();
    }
    
    copy_smtp(*v);
    if (*get_val(ctx.settings.smtp_poll)) {
      ctx.send_cond.notify_one();
    }

    if (try_save.errors.empty()) {
      db::commit(trans, "Saved Setup");
      log(v->ctx, "Saved setup");
      pop_view(v);
    }
  }
  
  static void on_imap(gpointer *_, Setup *v) {
    Ctx &ctx(v->ctx);
    db::Trans trans(v->ctx);
    copy_imap(*v);

    TRY(try_imap);
    Imap imap(ctx);
    if (try_imap.errors.empty()) { log(ctx, "Imap Ok"); }
  }

  static void on_smtp(gpointer *_, Setup *v) {
    Ctx &ctx(v->ctx);
    db::Trans trans(v->ctx);
    copy_smtp(*v);

    TRY(try_smtp);
    Smtp smtp(ctx);
    if (try_smtp.errors.empty()) { log(ctx, "Smtp Ok"); }
  }

  static GtkWidget *init_load_folder(Setup &v) {
    GtkWidget *frm(new_grid());
    gtk_widget_set_margin_top(frm, 5);

    gtk_grid_attach(GTK_GRID(frm), new_label("Load Folder"), 0, 0, 1, 1);
    gtk_widget_set_hexpand(v.load_folder, true);
    gtk_widget_set_sensitive(v.load_folder, false);
    gtk_grid_attach(GTK_GRID(frm), v.load_folder, 0, 1, 1, 1);

    GtkWidget *btn = gtk_button_new_with_label("Select");
    g_signal_connect(btn, "clicked", G_CALLBACK(on_load_folder), &v);
    gtk_grid_attach(GTK_GRID(frm), btn, 1, 1, 1, 1);
    return frm;
  }

  static GtkWidget *init_save_folder(Setup &v) {
    GtkWidget *frm(new_grid());
    gtk_grid_attach(GTK_GRID(frm), new_label("Save Folder"), 0, 0, 1, 1);
    gtk_widget_set_hexpand(v.save_folder, true);
    gtk_widget_set_sensitive(v.save_folder, false);
    gtk_grid_attach(GTK_GRID(frm), v.save_folder, 0, 1, 1, 1);

    GtkWidget *btn = gtk_button_new_with_label("Select");
    g_signal_connect(btn, "clicked", G_CALLBACK(on_save_folder), &v);
    gtk_grid_attach(GTK_GRID(frm), btn, 1, 1, 1, 1);
    return frm;
  }
  
  static GtkWidget *init_general(Setup &v) {
    GtkWidget *frm = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_top(frm, 5);

    gtk_container_add(GTK_CONTAINER(frm), new_label("Name"));
    gtk_widget_set_hexpand(v.name, true);
    gtk_container_add(GTK_CONTAINER(frm), v.name);
    
    gtk_container_add(GTK_CONTAINER(frm), new_label("Email"));
    gtk_widget_set_hexpand(v.email, true);
    gtk_container_add(GTK_CONTAINER(frm), v.email);

    gtk_container_add(GTK_CONTAINER(frm), init_load_folder(v));
    gtk_container_add(GTK_CONTAINER(frm), init_save_folder(v));
    return frm;
  }

  static GtkWidget *init_imap(Setup &v) {
    GtkWidget *frm(new_grid());
    gtk_widget_set_margin_top(frm, 5);

    int row(0);
    gtk_grid_attach(GTK_GRID(frm), new_label("URL"), 0, row, 2, 1);
    gtk_widget_set_hexpand(v.imap_url, true);
    gtk_grid_attach(GTK_GRID(frm), v.imap_url, 0, row+1, 2, 1);

    gtk_grid_attach(GTK_GRID(frm), new_label("SSL Port"), 2, row, 1, 1);
    gtk_grid_attach(GTK_GRID(frm), v.imap_port, 2, row+1, 1, 1);

    row += 2;
    gtk_grid_attach(GTK_GRID(frm), new_label("User"), 0, row, 1, 1);
    gtk_widget_set_hexpand(v.imap_user, true);
    gtk_grid_attach(GTK_GRID(frm), v.imap_user, 0, row+1, 1, 1);

    gtk_grid_attach(GTK_GRID(frm), new_label("Password"), 1, row, 1, 1);
    gtk_entry_set_visibility(GTK_ENTRY(v.imap_pass), false);
    gtk_widget_set_hexpand(v.imap_pass, true);
    gtk_grid_attach(GTK_GRID(frm), v.imap_pass, 1, row+1, 1, 1);

    gtk_grid_attach(GTK_GRID(frm), new_label("Poll (s)"), 2, row, 1, 1);
    gtk_grid_attach(GTK_GRID(frm), v.imap_poll, 2, row+1, 1, 1);

    row += 2;
    GtkWidget *btn = gtk_button_new_with_label("Connect");
    g_signal_connect(btn, "clicked", G_CALLBACK(on_imap), &v);
    gtk_grid_attach(GTK_GRID(frm), btn, 2, row, 1, 1);
    return frm;
  }

  static GtkWidget *init_smtp(Setup &v) {
    GtkWidget *frm(new_grid());
    gtk_widget_set_margin_top(frm, 5);

    int row(0);
    gtk_grid_attach(GTK_GRID(frm), new_label("URL"), 0, row, 2, 1);
    gtk_widget_set_hexpand(v.smtp_url, true);
    gtk_grid_attach(GTK_GRID(frm), v.smtp_url, 0, row+1, 2, 1);

    gtk_grid_attach(GTK_GRID(frm), new_label("SSL Port"), 2, row, 1, 1);
    gtk_grid_attach(GTK_GRID(frm), v.smtp_port, 2, row+1, 1, 1);

    row += 2;
    gtk_grid_attach(GTK_GRID(frm), new_label("User"), 0, row, 1, 1);
    gtk_widget_set_hexpand(v.smtp_user, true);
    gtk_grid_attach(GTK_GRID(frm), v.smtp_user, 0, row+1, 1, 1);

    gtk_grid_attach(GTK_GRID(frm), new_label("Password"), 1, row, 1, 1);
    gtk_entry_set_visibility(GTK_ENTRY(v.smtp_pass), false);
    gtk_widget_set_hexpand(v.smtp_pass, true);
    gtk_grid_attach(GTK_GRID(frm), v.smtp_pass, 1, row+1, 1, 1);

    gtk_grid_attach(GTK_GRID(frm), new_label("Poll (s)"), 2, row, 1, 1);
    gtk_grid_attach(GTK_GRID(frm), v.smtp_poll, 2, row+1, 1, 1);

    row += 2;
    GtkWidget *btn = gtk_button_new_with_label("Connect");
    g_signal_connect(btn, "clicked", G_CALLBACK(on_smtp), &v);
    gtk_grid_attach(GTK_GRID(frm), btn, 2, row, 1, 1);
    return frm;
  }

  Setup::Setup(Ctx &ctx):
    View(ctx, "Setup"),
    name(gtk_entry_new()),
    email(gtk_entry_new()),
    load_folder(gtk_entry_new()),
    save_folder(gtk_entry_new()),
    imap_url(gtk_entry_new()),
    imap_port(gtk_entry_new()),
    imap_user(gtk_entry_new()),
    imap_pass(gtk_entry_new()),
    imap_poll(gtk_entry_new()),
    smtp_url(gtk_entry_new()),
    smtp_port(gtk_entry_new()),
    smtp_user(gtk_entry_new()),
    smtp_pass(gtk_entry_new()),
    smtp_poll(gtk_entry_new()),
    save(gtk_button_new_with_mnemonic("_Save Setup")),
    cancel(gtk_button_new_with_mnemonic("_Cancel"))
  {
    GtkWidget *tabs = gtk_notebook_new();
    gtk_widget_set_vexpand(tabs, true);
    gtk_notebook_append_page(GTK_NOTEBOOK(tabs),
			     init_general(*this),
			     gtk_label_new_with_mnemonic("_1 General"));
    gtk_notebook_append_page(GTK_NOTEBOOK(tabs),
			     init_imap(*this),
			     gtk_label_new_with_mnemonic("_2 Imap"));
    gtk_notebook_append_page(GTK_NOTEBOOK(tabs),
			     init_smtp(*this),
			     gtk_label_new_with_mnemonic("_3 Smtp"));
    gtk_container_add(GTK_CONTAINER(panel), tabs);
    
    GtkWidget *btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_halign(btns, GTK_ALIGN_END);
    gtk_widget_set_valign(btns, GTK_ALIGN_END);
    gtk_container_add(GTK_CONTAINER(panel), btns);
    
    g_signal_connect(save, "clicked", G_CALLBACK(on_save), this);
    gtk_container_add(GTK_CONTAINER(btns), save);

    g_signal_connect(cancel, "clicked", G_CALLBACK(on_cancel), this);
    gtk_container_add(GTK_CONTAINER(btns), cancel);    
    focused = name;
  }

  void Setup::load() {
    View::load();
    
    Peer &me(whoami(ctx));
    gtk_entry_set_text(GTK_ENTRY(name), me.name.c_str());
    gtk_entry_set_text(GTK_ENTRY(email), me.email.c_str());

    gtk_entry_set_text(GTK_ENTRY(load_folder),
		       get_val(ctx.settings.load_folder)->c_str());    
    gtk_entry_set_text(GTK_ENTRY(save_folder),
		       get_val(ctx.settings.save_folder)->c_str());

    gtk_entry_set_text(GTK_ENTRY(imap_url),
		       get_val(ctx.settings.imap_url)->c_str());
    gtk_entry_set_text(GTK_ENTRY(imap_port),
		       to_str(*get_val(ctx.settings.imap_port)).c_str());
    gtk_entry_set_text(GTK_ENTRY(imap_user),
		       get_val(ctx.settings.imap_user)->c_str());
    gtk_entry_set_text(GTK_ENTRY(imap_pass),
		       get_val(ctx.settings.imap_pass)->c_str());
    gtk_entry_set_text(GTK_ENTRY(imap_poll),
		       to_str(*get_val(ctx.settings.imap_poll)).c_str());
    
    gtk_entry_set_text(GTK_ENTRY(smtp_url),
		       get_val(ctx.settings.smtp_url)->c_str());
    gtk_entry_set_text(GTK_ENTRY(smtp_port),
		       to_str(*get_val(ctx.settings.smtp_port)).c_str());
    gtk_entry_set_text(GTK_ENTRY(smtp_user),
		       get_val(ctx.settings.smtp_user)->c_str());
    gtk_entry_set_text(GTK_ENTRY(smtp_pass),
		       get_val(ctx.settings.smtp_pass)->c_str());
    gtk_entry_set_text(GTK_ENTRY(smtp_poll),
		       to_str(*get_val(ctx.settings.smtp_poll)).c_str());
  }
}}

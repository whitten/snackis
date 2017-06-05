#ifndef SNACKIS_UI_PROFILE_FORM_HPP
#define SNACKIS_UI_PROFILE_FORM_HPP

#include "snackis/ui/view_form.hpp"
#include "snackis/ui/text_field.hpp"

namespace snackis {
namespace ui {
  struct ProfileForm: public ViewForm {
    TextField name, email, editor,
      imap_url, imap_port, imap_user, imap_pass,
      smtp_url, smtp_port, smtp_user, smtp_pass;
    
    ProfileForm(View &view, Footer &ftr);
  };

  bool run(ProfileForm &frm);
}}

#endif